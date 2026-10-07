#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "encode/encode_client.h"
#include "encode/host/encode_session.h"
#include "encode_host_generated.h"
#include "media/media_format.h"
#include "media_sink.h"

#include <flatbuffers/buffer.h>
#include <flatbuffers/flatbuffer_builder.h>
#include <flatbuffers/verifier.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr int kSrcW = 64;
constexpr int kSrcH = 48;
constexpr int kFrames = 6;
constexpr std::array<unsigned char, 4> kEbmlMagic{0x1A, 0x45, 0xDF, 0xA3};
constexpr std::size_t kPngHeadBytes = 24;

std::wstring HostExe() {
    const std::string narrow = R573_ENCODE_HOST_EXE;
    return {narrow.begin(), narrow.end()};
}

fs::path OutDir(const char* name) {
    const fs::path dir = fs::temp_directory_path() / "r573_encode_host_tests" / name;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

std::vector<uint8_t> Frame(int index) {
    std::vector<uint8_t> bgra(static_cast<std::size_t>(kSrcW) * kSrcH * 4);
    for (std::size_t i = 0; i < bgra.size(); i++)
        bgra[i] = static_cast<uint8_t>((i * 7 + static_cast<std::size_t>(index) * 31) & 0xFFU);
    return bgra;
}

MediaSink::Params Params(MediaSink::Format format, const fs::path& output) {
    MediaSink::Params p;
    p.output_path = output.string();
    p.format = format;
    p.src_width = kSrcW;
    p.src_height = kSrcH;
    p.fps = 30;
    p.quality = 30;
    p.keyframe_interval = 3;
    p.prefer_hardware = false;
    return p;
}

std::string Encode(EncodeClient::Sink& sink, const MediaSink::Params& p) {
    if (!sink.Open(p)) return "Open: " + sink.LastError();
    for (int i = 0; i < kFrames; i++) {
        const std::vector<uint8_t> frame = Frame(i);
        if (!sink.SubmitFrame(frame.data(), i)) return "SubmitFrame: " + sink.LastError();
    }
    if (!sink.Finish()) return "Finish: " + sink.LastError();
    return {};
}

std::vector<unsigned char> Head(const fs::path& file, std::size_t count) {
    std::ifstream reading(file, std::ios::binary);
    std::vector<char> bytes(count);
    reading.read(bytes.data(), static_cast<std::streamsize>(count));
    bytes.resize(static_cast<std::size_t>(reading.gcount()));
    return {bytes.begin(), bytes.end()};
}

std::pair<int, int> PngSize(const fs::path& file) {
    const std::vector<unsigned char> head = Head(file, kPngHeadBytes);
    if (head.size() != kPngHeadBytes) return {0, 0};
    const auto big = [&head](std::size_t at) {
        return (head.at(at) << 24U) | (head.at(at + 1) << 16U) | (head.at(at + 2) << 8U) |
               head.at(at + 3);
    };
    return {big(16), big(20)};
}

const EncodeProtocol::ReplyMessage* Answer(EncodeHost::Session& session,
                                           flatbuffers::FlatBufferBuilder& builder,
                                           std::vector<uint8_t>& reply) {
    reply = session.Handle(std::span<const uint8_t>(builder.GetBufferPointer(), builder.GetSize()));
    flatbuffers::Verifier verifier(reply.data(), reply.size());
    REQUIRE(verifier.VerifyBuffer<EncodeProtocol::ReplyMessage>(nullptr));
    return flatbuffers::GetRoot<EncodeProtocol::ReplyMessage>(reply.data());
}

}

TEST_CASE("The encoder child process writes every frame the renderer hands it") {
    EncodeClient::Host host(HostExe());

    const fs::path frames_dir = OutDir("png");
    MediaSink::Params png = Params(MediaSink::Format::PNG_Sequence, frames_dir);
    png.out_width = kSrcW / 2;
    png.out_height = kSrcH / 2;
    EncodeClient::Sink png_sink(host);
    REQUIRE(Encode(png_sink, png).empty());
    for (int i = 0; i < kFrames; i++) {
        const fs::path frame = frames_dir / std::format("frame_{:06}.png", i);
        INFO(frame.string());
        CHECK(PngSize(frame) == std::pair{kSrcW / 2, kSrcH / 2});
    }

    const fs::path webm = OutDir("webm") / "out.webm";
    EncodeClient::Sink webm_sink(host);
    REQUIRE(Encode(webm_sink, Params(MediaSink::Format::WebM_VP9, webm)).empty());
    CHECK_FALSE(webm_sink.UsingHardware());
    CHECK(Head(webm, kEbmlMagic.size()) ==
          std::vector<unsigned char>(kEbmlMagic.begin(), kEbmlMagic.end()));
}

TEST_CASE("An encoder failure comes back as the sink's error and the child keeps serving") {
    EncodeClient::Host host(HostExe());

    EncodeClient::Sink broken(host);
    const fs::path nowhere = OutDir("broken") / "missing_dir" / "out.webm";
    CHECK_FALSE(broken.Open(Params(MediaSink::Format::WebM_VP9, nowhere)));
    INFO(broken.LastError());
    CHECK_FALSE(broken.LastError().empty());

    EncodeClient::Sink working(host);
    const fs::path webm = OutDir("after_failure") / "out.webm";
    CHECK(Encode(working, Params(MediaSink::Format::WebM_VP9, webm)).empty());
}

TEST_CASE("A missing encoder executable is named in the sink's error") {
    EncodeClient::Host host(L"C:\\no\\such\\573Encoder.exe");
    EncodeClient::Sink sink(host);
    CHECK_FALSE(sink.Open(Params(MediaSink::Format::WebM_VP9, OutDir("missing") / "out.webm")));
    INFO(sink.LastError());
    CHECK(sink.LastError().find("573Encoder.exe") != std::string::npos);
    CHECK_FALSE(host.HardwareAvailable(MediaSink::Format::MP4_H264));
}

TEST_CASE("The encoder session answers a hardware probe and refuses unknown sessions") {
    EncodeHost::Session session;
    std::vector<uint8_t> reply;

    flatbuffers::FlatBufferBuilder probe;
    probe.Finish(EncodeProtocol::CreateRequestMessage(
        probe, EncodeProtocol::Request::Probe,
        EncodeProtocol::CreateProbe(probe, MediaSink::ToIndex(MediaSink::Format::MP4_H264))
            .Union()));
    CHECK(Answer(session, probe, reply)->reply_type() == EncodeProtocol::Reply::Probed);

    flatbuffers::FlatBufferBuilder frame;
    frame.Finish(EncodeProtocol::CreateRequestMessage(
        frame, EncodeProtocol::Request::Frame, EncodeProtocol::CreateFrame(frame, 99, 0).Union()));
    const auto* refused = Answer(session, frame, reply);
    REQUIRE(refused->reply_type() == EncodeProtocol::Reply::Failure);
    CHECK(refused->reply_as_Failure()->message()->str() == "no session 99");
}
