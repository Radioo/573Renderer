#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "media/movie_source.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libavcodec/codec_id.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
}

namespace {

constexpr int kWidth = 96;
constexpr int kHeight = 64;
constexpr int kFrames = 12;
constexpr int kFps = 25;
constexpr int kLumaBase = 40;
constexpr int kLumaStep = 15;

int LumaOf(int index) {
    return kLumaBase + (index * kLumaStep);
}

bool Encode(AVFormatContext* fmt, AVCodecContext* enc, AVStream* stream, AVFrame* frame) {
    if (avcodec_send_frame(enc, frame) < 0) return false;
    while (true) {
        AVPacket* packet = av_packet_alloc();
        const int rc = avcodec_receive_packet(enc, packet);
        if (rc < 0) {
            av_packet_free(&packet);
            return true;
        }
        av_packet_rescale_ts(packet, enc->time_base, stream->time_base);
        packet->stream_index = stream->index;
        av_interleaved_write_frame(fmt, packet);
        av_packet_free(&packet);
    }
}

void FillPlane(std::uint8_t* base, int stride, int width, int height, std::uint8_t value) {
    const std::span<std::uint8_t> plane(base, (std::size_t)stride * (std::size_t)height);
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++)
            plane[((std::size_t)row * (std::size_t)stride) + (std::size_t)col] = value;
    }
}

bool WriteProgramStream(const std::string& path) {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO);
    if (codec == nullptr) return false;
    AVFormatContext* fmt = nullptr;
    if (avformat_alloc_output_context2(&fmt, nullptr, "mpeg", path.c_str()) < 0) return false;
    AVStream* stream = avformat_new_stream(fmt, nullptr);
    AVCodecContext* enc = avcodec_alloc_context3(codec);
    enc->width = kWidth;
    enc->height = kHeight;
    enc->pix_fmt = AV_PIX_FMT_YUV420P;
    enc->time_base = AVRational{1, kFps};
    enc->framerate = AVRational{kFps, 1};
    enc->gop_size = 4;
    enc->max_b_frames = 0;
    enc->bit_rate = 2000000;
    if (avcodec_open2(enc, codec, nullptr) < 0) return false;
    avcodec_parameters_from_context(stream->codecpar, enc);
    stream->time_base = enc->time_base;
    if (avio_open(&fmt->pb, path.c_str(), AVIO_FLAG_WRITE) < 0) return false;
    if (avformat_write_header(fmt, nullptr) < 0) return false;

    AVFrame* frame = av_frame_alloc();
    frame->format = enc->pix_fmt;
    frame->width = kWidth;
    frame->height = kHeight;
    av_frame_get_buffer(frame, 0);
    const std::span<std::uint8_t* const> planes(frame->data);
    const std::span<const int> strides(frame->linesize);
    for (int index = 0; index < kFrames; index++) {
        av_frame_make_writable(frame);
        FillPlane(planes[0], strides[0], kWidth, kHeight, (std::uint8_t)LumaOf(index));
        for (std::size_t plane = 1; plane < 3; plane++)
            FillPlane(planes[plane], strides[plane], kWidth / 2, kHeight / 2, 128);
        frame->pts = index;
        if (!Encode(fmt, enc, stream, frame)) return false;
    }
    Encode(fmt, enc, stream, nullptr);
    av_write_trailer(fmt);
    av_frame_free(&frame);
    avcodec_free_context(&enc);
    avio_closep(&fmt->pb);
    avformat_free_context(fmt);
    return true;
}

std::string FixturePath() {
    static const std::string path =
        (std::filesystem::temp_directory_path() / "r573_movie_source_fixture.4").string();
    static const bool written = WriteProgramStream(path);
    REQUIRE(written);
    return path;
}

int CentreGrey(const Movie::Frame& frame) {
    if (frame.bgra == nullptr) return -1;
    const auto width = (std::size_t)frame.width;
    const auto height = (std::size_t)frame.height;
    const std::span<const std::uint8_t> pixels(frame.bgra, width * height * 4U);
    const std::size_t at = (((height / 2U) * width) + (width / 2U)) * 4U;
    return (int)pixels[at + 1U];
}

std::vector<int> WalkGreys(Movie::Source& source) {
    std::vector<int> greys;
    greys.reserve(kFrames);
    for (int index = 0; index < kFrames; index++)
        greys.push_back(CentreGrey(source.FrameAt(index)));
    return greys;
}

}

TEST_CASE("a movie source reads an MPEG program stream and reports its own rate", "[movie]") {
    Movie::Source source;
    INFO(source.LastError());
    REQUIRE(source.Open(FixturePath()));
    CHECK(source.IsOpen());
    CHECK(source.FrameRate() == 25.0);

    const Movie::Frame first = source.FrameAt(0);
    REQUIRE(first.bgra != nullptr);
    CHECK(first.width == kWidth);
    CHECK(first.height == kHeight);
    CHECK(CentreGrey(first) > 0);
}

TEST_CASE("a movie source walks forward frame by frame and seeks back to the start", "[movie]") {
    Movie::Source source;
    REQUIRE(source.Open(FixturePath()));
    const std::vector<int> greys = WalkGreys(source);
    REQUIRE(greys.size() == (std::size_t)kFrames);

    INFO("the fixture brightens every frame, so a decoder that stalls repeats a value");
    for (std::size_t i = 1; i < greys.size(); i++) {
        INFO("frame " << i);
        CHECK(greys[i] > greys[i - 1]);
    }

    INFO("scrubbing back rewinds the stream instead of holding the newest frame");
    CHECK(CentreGrey(source.FrameAt(0)) == greys.front());
    CHECK(CentreGrey(source.FrameAt(3)) == greys[3]);
    CHECK(CentreGrey(source.FrameAt(kFrames - 1)) == greys.back());
}

TEST_CASE("a movie source holds its last frame past the end of the stream", "[movie]") {
    Movie::Source source;
    REQUIRE(source.Open(FixturePath()));
    const std::vector<int> greys = WalkGreys(source);
    INFO("the game's player has no EC_COMPLETE handler, so the texture keeps the last picture");
    CHECK(CentreGrey(source.FrameAt(kFrames)) == greys.back());
    CHECK(CentreGrey(source.FrameAt(kFrames + 40)) == greys.back());
    CHECK(CentreGrey(source.FrameAt(kFrames * 8)) == greys.back());
    INFO("holding is not looping: the first frame is never shown again on its own");
    CHECK(greys.back() != greys.front());
}

TEST_CASE("a movie source reports the frame it holds rather than the one asked for", "[movie]") {
    Movie::Source source;
    REQUIRE(source.Open(FixturePath()));
    CHECK(source.Position() == -1);
    CHECK(source.FrameAt(0).bgra != nullptr);
    CHECK(source.Position() == 0);
    CHECK(source.FrameAt(5).bgra != nullptr);
    CHECK(source.Position() == 5);

    INFO("past the end the held frame stops climbing even though the request does not, so a "
         "consumer keyed on the request would re-upload the same picture for ever");
    CHECK(source.FrameAt(kFrames * 4).bgra != nullptr);
    CHECK(source.Position() == kFrames - 1);
    CHECK(source.FrameAt(kFrames * 8).bgra != nullptr);
    CHECK(source.Position() == kFrames - 1);

    INFO("a stream that decodes normally never latches the terminal scaler failure");
    CHECK_FALSE(source.Broken());
}

TEST_CASE("a movie source reports every decode step so a long replay can show progress",
          "[movie]") {
    Movie::Source source;
    REQUIRE(source.Open(FixturePath()));

    std::vector<int> forward;
    source.FrameAt(kFrames - 1, [&](int decoded, int wanted) {
        CHECK(wanted == kFrames - 1);
        forward.push_back(decoded);
    });
    REQUIRE(forward.size() == (std::size_t)kFrames);
    CHECK(forward.front() == 0);
    CHECK(forward.back() == kFrames - 1);

    std::vector<int> back;
    source.FrameAt(2, [&](int decoded, int) { back.push_back(decoded); });
    INFO("a backwards scrub replays from frame 0, which is the run that used to be silent");
    CHECK(back == std::vector<int>{0, 1, 2});

    std::vector<int> to_end;
    source.FrameAt(kFrames * 4, [&](int decoded, int) { to_end.push_back(decoded); });
    CHECK(to_end.size() == (std::size_t)(kFrames - 3));

    std::vector<int> idle;
    source.FrameAt(kFrames * 4, [&](int decoded, int) { idle.push_back(decoded); });
    INFO("past the end nothing decodes, so a held picture never raises the loading overlay");
    CHECK(idle.empty());
}

TEST_CASE("a movie source maps document seconds onto its own frame numbering", "[movie]") {
    Movie::Source source;
    REQUIRE(source.Open(FixturePath()));
    CHECK(source.IndexAt(0.0) == 0);
    CHECK(source.IndexAt(1.0 / 60.0) == 0);
    CHECK(source.IndexAt(4.0 / 60.0) == 1);
    CHECK(source.IndexAt(1.0) == 25);
    CHECK(source.IndexAt(-1.0) == 0);
}

TEST_CASE("a movie source scales its output to the size the tiles expect", "[movie]") {
    Movie::Source plain;
    REQUIRE(plain.Open(FixturePath()));
    const int native = CentreGrey(plain.FrameAt(2));

    Movie::Source source;
    REQUIRE(source.Open(FixturePath()));
    source.SetOutputSize(304, 416);
    const Movie::Frame frame = source.FrameAt(2);
    REQUIRE(frame.bgra != nullptr);
    CHECK(frame.width == 304);
    CHECK(frame.height == 416);
    INFO("the bilinear resample keeps a flat field flat to within a level");
    CHECK(std::abs(CentreGrey(frame) - native) <= 1);
}

TEST_CASE("a missing movie file fails to open and never claims a frame", "[movie]") {
    Movie::Source source;
    const std::string missing =
        (std::filesystem::temp_directory_path() / "r573_movie_not_here.4").string();
    std::filesystem::remove(missing);
    CHECK_FALSE(source.Open(missing));
    CHECK_FALSE(source.IsOpen());
    CHECK_FALSE(source.LastError().empty());
    CHECK(source.FrameAt(0).bgra == nullptr);
    CHECK(source.FrameRate() == 0.0);
}
