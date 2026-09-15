#include <catch2/catch_test_macros.hpp>

#include "preview/preview_channel.h"
#include "preview_host_generated.h"
#include "support/com_ptr.h"
#include "support/env.h"

#include <flatbuffers/buffer.h>
#include <flatbuffers/flatbuffer_builder.h>
#include <flatbuffers/verifier.h>

#include <windows.h>

#include <d3d9.h>

#include <bit>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr unsigned kConnectMs = 20000;
constexpr unsigned kReplyMs = 120000;
constexpr uint32_t kSeekFrame = 300;
constexpr uint32_t kViewWidth = 640;
constexpr uint32_t kViewHeight = 360;

std::vector<uint8_t> Finished(const flatbuffers::FlatBufferBuilder& builder) {
    const std::span<const uint8_t> bytes(builder.GetBufferPointer(), builder.GetSize());
    return {bytes.begin(), bytes.end()};
}

template <typename Make> std::vector<uint8_t> Request(PreviewProtocol::Request type, Make make) {
    flatbuffers::FlatBufferBuilder builder;
    const auto body = make(builder);
    builder.Finish(PreviewProtocol::CreateRequestMessage(builder, type, body.Union()));
    return Finished(builder);
}

const PreviewProtocol::ReplyMessage* Verified(const std::vector<uint8_t>& reply) {
    flatbuffers::Verifier verifier(reply.data(), reply.size());
    REQUIRE(verifier.VerifyBuffer<PreviewProtocol::ReplyMessage>(nullptr));
    const auto* message = flatbuffers::GetRoot<PreviewProtocol::ReplyMessage>(reply.data());
    if (const auto* failure = message->reply_as_Failure()) FAIL(failure->message()->str());
    return message;
}

std::vector<uint8_t> ReadHostFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

bool OpensOnAnotherDevice(uint64_t handle_bits, uint32_t width, uint32_t height) {
    ComPtr<IDirect3D9Ex> d3d;
    if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d))) return false;
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.hDeviceWindow = GetDesktopWindow();
    ComPtr<IDirect3DDevice9Ex> device;
    if (FAILED(d3d->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, pp.hDeviceWindow,
                                   D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, nullptr, &device))) {
        return false;
    }
    ComPtr<IDirect3DTexture9> texture;
    auto* shared = std::bit_cast<HANDLE>(static_cast<uintptr_t>(handle_bits));
    return SUCCEEDED(device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                           D3DPOOL_DEFAULT, &texture, &shared));
}

}

TEST_CASE("The preview host boots, loads a package from bytes, seeks and shares its frame") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    const std::string pipe_name =
        std::format(R"(\\.\pipe\r573_preview_host_test_{})", GetCurrentProcessId());
    const std::string narrow_command =
        std::format(R"("{}" "{}")", R573_PREVIEW_HOST_EXE, pipe_name);
    std::wstring command(narrow_command.begin(), narrow_command.end());
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    REQUIRE(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                           nullptr, nullptr, &startup, &process) != FALSE);

    auto pipe = PreviewChannel::Connect(pipe_name, kConnectMs);
    REQUIRE(pipe.has_value());
    PreviewChannel::Client client(std::move(*pipe), kReplyMs);

    const auto booted =
        client.Call(Request(PreviewProtocol::Request::Boot,
                            [&](flatbuffers::FlatBufferBuilder& b) {
                                return PreviewProtocol::CreateBoot(b, b.CreateString(dir),
                                                                   b.CreateString("iidx33"));
                            }),
                    "Boot");
    REQUIRE(booted.has_value());
    CHECK(Verified(*booted)->reply_as_Done() != nullptr);

    const std::vector<uint8_t> ifs = ReadHostFile(dir + "/data/graphic/1/title.ifs");
    REQUIRE(!ifs.empty());
    const auto loaded_reply =
        client.Call(Request(PreviewProtocol::Request::LoadPackage,
                            [&](flatbuffers::FlatBufferBuilder& b) {
                                return PreviewProtocol::CreateLoadPackage(
                                    b, b.CreateString("title"), b.CreateString("title"),
                                    b.CreateVector(ifs), false);
                            }),
                    "LoadPackage");
    REQUIRE(loaded_reply.has_value());
    const auto* loaded = Verified(*loaded_reply)->reply_as_Loaded();
    REQUIRE(loaded != nullptr);
    CHECK(loaded->frame_count() == 840);
    REQUIRE(loaded->labels() != nullptr);
    REQUIRE(loaded->labels()->size() == 1);
    CHECK(loaded->labels()->Get(0)->name()->str() == "loop");
    CHECK(loaded->labels()->Get(0)->frame() == 240);

    const auto sought = client.Call(Request(PreviewProtocol::Request::Seek,
                                            [&](flatbuffers::FlatBufferBuilder& b) {
                                                return PreviewProtocol::CreateSeek(b, kSeekFrame);
                                            }),
                                    "Seek");
    REQUIRE(sought.has_value());
    CHECK(Verified(*sought)->reply_as_Done() != nullptr);

    const auto resized =
        client.Call(Request(PreviewProtocol::Request::Resize,
                            [&](flatbuffers::FlatBufferBuilder& b) {
                                return PreviewProtocol::CreateResize(b, kViewWidth, kViewHeight);
                            }),
                    "Resize");
    REQUIRE(resized.has_value());
    CHECK(Verified(*resized)->reply_as_Done() != nullptr);

    const auto rendered = client.Call(Request(PreviewProtocol::Request::Render,
                                              [&](flatbuffers::FlatBufferBuilder& b) {
                                                  return PreviewProtocol::CreateRender(b);
                                              }),
                                      "Render");
    REQUIRE(rendered.has_value());
    const auto* frame = Verified(*rendered)->reply_as_Frame();
    REQUIRE(frame != nullptr);
    CHECK(frame->width() == kViewWidth);
    CHECK(frame->height() == kViewHeight);
    CHECK(frame->frame() == kSeekFrame);
    CHECK(frame->shared_handle() != 0);
    CHECK(OpensOnAnotherDevice(frame->shared_handle(), frame->width(), frame->height()));

    client = PreviewChannel::Client(PreviewChannel::Pipe(), kReplyMs);
    CHECK(WaitForSingleObject(process.hProcess, 30000) == WAIT_OBJECT_0);
    DWORD exit_code = 1;
    GetExitCodeProcess(process.hProcess, &exit_code);
    CHECK(exit_code == 0);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
}
