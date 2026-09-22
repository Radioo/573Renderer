#include <catch2/catch_test_macros.hpp>

#include "preview/preview_channel.h"
#include "preview_host_generated.h"

#include <flatbuffers/buffer.h>
#include <flatbuffers/flatbuffer_builder.h>
#include <flatbuffers/verifier.h>

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr unsigned kTimeoutMs = 5000;

std::string PipeName(const char* test) {
    return std::format(R"(\\.\pipe\r573_preview_test_{}_{})", test, GetCurrentProcessId());
}

struct Connected {
    PreviewChannel::Pipe server;
    PreviewChannel::Pipe client;
};

Connected Connect(const char* test) {
    const std::string name = PipeName(test);
    auto server = PreviewChannel::Listen(name);
    REQUIRE(server.has_value());
    auto client = PreviewChannel::Connect(name, kTimeoutMs);
    REQUIRE(client.has_value());
    REQUIRE(PreviewChannel::Accept(*server, kTimeoutMs).has_value());
    return Connected{.server = std::move(*server), .client = std::move(*client)};
}

std::vector<uint8_t> Finished(const flatbuffers::FlatBufferBuilder& builder) {
    const std::span<const uint8_t> bytes(builder.GetBufferPointer(), builder.GetSize());
    return {bytes.begin(), bytes.end()};
}

std::vector<uint8_t> LoadPackageRequest(const std::vector<uint8_t>& ifs) {
    flatbuffers::FlatBufferBuilder builder;
    const auto package = builder.CreateString("title");
    const auto animation = builder.CreateString("title");
    const auto bytes = builder.CreateVector(ifs);
    const auto load = PreviewProtocol::CreateLoadPackage(builder, package, animation, bytes, true);
    builder.Finish(PreviewProtocol::CreateRequestMessage(
        builder, PreviewProtocol::Request::LoadPackage, load.Union()));
    return Finished(builder);
}

std::vector<uint8_t> LoadedReply() {
    flatbuffers::FlatBufferBuilder builder;
    const std::vector<flatbuffers::Offset<PreviewProtocol::Label>> labels = {
        PreviewProtocol::CreateLabel(builder, builder.CreateString("loop"), 240)};
    const auto loaded = PreviewProtocol::CreateLoaded(builder, 840, builder.CreateVector(labels));
    builder.Finish(PreviewProtocol::CreateReplyMessage(builder, PreviewProtocol::Reply::Loaded,
                                                       loaded.Union()));
    return Finished(builder);
}

std::vector<uint8_t> SeekRequest(uint32_t frame) {
    flatbuffers::FlatBufferBuilder builder;
    const auto seek = PreviewProtocol::CreateSeek(builder, frame);
    builder.Finish(PreviewProtocol::CreateRequestMessage(builder, PreviewProtocol::Request::Seek,
                                                         seek.Union()));
    return Finished(builder);
}

}

TEST_CASE("A package request and its reply travel whole over a named pipe") {
    Connected pipes = Connect("roundtrip");
    std::vector<uint8_t> ifs(std::size_t{5} * 1024 * 1024);
    for (std::size_t i = 0; i < ifs.size(); i++)
        ifs[i] = static_cast<uint8_t>(i * 7U);

    std::string host_problem;
    std::vector<uint8_t> received;
    bool host_threw = false;
    std::thread host([&]() noexcept {
        try {
            auto request = PreviewChannel::Receive(pipes.server, kTimeoutMs);
            if (!request) {
                host_problem = request.error();
                return;
            }
            received = std::move(*request);
            auto sent = PreviewChannel::Send(pipes.server, LoadedReply());
            if (!sent) host_problem = sent.error();
        } catch (...) {
            host_threw = true;
        }
    });

    REQUIRE(PreviewChannel::Send(pipes.client, LoadPackageRequest(ifs)).has_value());
    auto reply = PreviewChannel::Receive(pipes.client, kTimeoutMs);
    host.join();
    REQUIRE_FALSE(host_threw);
    REQUIRE(host_problem.empty());
    flatbuffers::Verifier verifier(received.data(), received.size());
    REQUIRE(PreviewProtocol::VerifyRequestMessageBuffer(verifier));
    const auto* load =
        PreviewProtocol::GetRequestMessage(received.data())->request_as_LoadPackage();
    REQUIRE(load != nullptr);
    CHECK(load->package()->str() == "title");
    CHECK(load->reload());
    CHECK(std::vector<uint8_t>(load->ifs()->begin(), load->ifs()->end()) == ifs);
    REQUIRE(reply.has_value());
    const auto* loaded =
        flatbuffers::GetRoot<PreviewProtocol::ReplyMessage>(reply->data())->reply_as_Loaded();
    REQUIRE(loaded != nullptr);
    CHECK(loaded->frame_count() == 840);
    REQUIRE(loaded->labels()->size() == 1);
    CHECK(loaded->labels()->Get(0)->name()->str() == "loop");
    CHECK(loaded->labels()->Get(0)->frame() == 240);
}

TEST_CASE("Receive gives up after its timeout") {
    const Connected pipes = Connect("timeout");
    const auto reply = PreviewChannel::Receive(pipes.client, 50);
    REQUIRE_FALSE(reply.has_value());
    CHECK(reply.error().find("timed out") != std::string::npos);
}

TEST_CASE("A peer that went away is reported") {
    Connected pipes = Connect("closed");
    pipes.server.Close();
    const auto reply = PreviewChannel::Receive(pipes.client, kTimeoutMs);
    REQUIRE_FALSE(reply.has_value());
    CHECK(reply.error().find("closed") != std::string::npos);
}

TEST_CASE("The client names the request the host never answered") {
    Connected pipes = Connect("crash");
    PreviewChannel::Client client(std::move(pipes.client), kTimeoutMs);
    bool host_received = false;
    std::thread host([&]() noexcept {
        try {
            host_received = PreviewChannel::Receive(pipes.server, kTimeoutMs).has_value();
        } catch (...) {
            host_received = false;
        }
        pipes.server.Close();
    });
    const auto reply = client.Call(SeekRequest(300), "Seek");
    host.join();
    CHECK(host_received);
    REQUIRE_FALSE(reply.has_value());
    CHECK(reply.error().find("Seek") != std::string::npos);
    CHECK(client.LastRequest() == "Seek");
}

TEST_CASE("Receive refuses a length prefix larger than any message") {
    const Connected pipes = Connect("oversize");
    const std::vector<uint8_t> prefix = {0xFF, 0xFF, 0xFF, 0x7F};
    DWORD written = 0;
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    const BOOL ok = WriteFile(pipes.server.Get(), prefix.data(), 4, &written, &overlapped);
    if (ok == FALSE && GetLastError() == ERROR_IO_PENDING)
        GetOverlappedResult(pipes.server.Get(), &overlapped, &written, TRUE);
    CloseHandle(overlapped.hEvent);
    const auto reply = PreviewChannel::Receive(pipes.client, kTimeoutMs);
    REQUIRE_FALSE(reply.has_value());
    CHECK(reply.error().find("too large") != std::string::npos);
}
