#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "preview/preview_client.h"
#include "preview/shared_texture.h"
#include "support/com_ptr.h"
#include "support/env.h"

#include <windows.h>

#include <d3d9.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kSeekFrame = 300;
constexpr uint32_t kViewWidth = 640;
constexpr uint32_t kViewHeight = 360;
constexpr uint32_t kTitleFrames = 840;
constexpr uint32_t kLoopFrame = 240;

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

TEST_CASE("The client drives a real preview host through a package, a seek and a frame") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    auto host =
        PreviewClient::Host::Start(PreviewClient::Options{.host_exe = R573_PREVIEW_HOST_EXE});
    REQUIRE(host.has_value());

    const auto booted = (*host)->Boot(dir, "iidx33");
    const std::string boot_error = booted.has_value() ? std::string() : booted.error();
    INFO(boot_error);
    REQUIRE(booted.has_value());

    const std::vector<uint8_t> ifs = ReadHostFile(dir + "/data/graphic/1/title.ifs");
    REQUIRE(!ifs.empty());
    const auto loaded = (*host)->LoadPackage("title", "title", ifs, false);
    const std::string load_error = loaded.has_value() ? std::string() : loaded.error();
    INFO(load_error);
    REQUIRE(loaded.has_value());
    CHECK(loaded->frame_count == kTitleFrames);
    REQUIRE(loaded->labels.size() == 1);
    CHECK(loaded->labels[0].name == "loop");
    CHECK(loaded->labels[0].frame == kLoopFrame);

    REQUIRE((*host)->Seek(kSeekFrame).has_value());
    REQUIRE((*host)->Resize(kViewWidth, kViewHeight).has_value());
    const auto frame = (*host)->Render();
    REQUIRE(frame.has_value());
    CHECK(frame->width == kViewWidth);
    CHECK(frame->height == kViewHeight);
    CHECK(frame->frame == kSeekFrame);
    CHECK(frame->stage_width == 1920);
    CHECK(frame->stage_height == 1080);
    CHECK(OpensOnAnotherDevice(frame->shared_handle, frame->width, frame->height));

    auto reader = SharedTexture::Reader::Create();
    REQUIRE(reader.has_value());
    const auto pixels = reader->Read(frame->shared_handle, frame->width, frame->height);
    const std::string pixel_error = pixels.has_value() ? std::string() : pixels.error();
    INFO(pixel_error);
    REQUIRE(pixels.has_value());
    REQUIRE(pixels->size() == static_cast<std::size_t>(frame->width) * frame->height * 4);
    CHECK(std::ranges::any_of(*pixels, [](uint8_t value) { return value != 0; }));

    const auto reloaded = (*host)->LoadPackage("title", "title", ifs, true);
    REQUIRE(reloaded.has_value());
    CHECK(reloaded->frame_count == kTitleFrames);
}

TEST_CASE("The client reports a host that refuses a request") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    auto host =
        PreviewClient::Host::Start(PreviewClient::Options{.host_exe = R573_PREVIEW_HOST_EXE});
    REQUIRE(host.has_value());
    const auto booted = (*host)->Boot(dir, "not_a_build");
    REQUIRE_FALSE(booted.has_value());
    CHECK(booted.error().find("not_a_build") != std::string::npos);
    CHECK((*host)->LastRequest() == "Boot");
}

TEST_CASE("Starting a client fails when the host executable is missing") {
    const auto host = PreviewClient::Host::Start(
        PreviewClient::Options{.host_exe = "no_such_preview_host.exe", .connect_timeout_ms = 1000});
    REQUIRE_FALSE(host.has_value());
}
