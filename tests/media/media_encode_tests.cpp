#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "media/media_format.h"
#include "media_sink.h"

extern "C" {
#include <libavcodec/codec_par.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

#include <cstddef>
#include <cstdint>
#include <span>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr int kSrcW = 64;
constexpr int kSrcH = 48;
constexpr int kFrames = 12;

fs::path OutDir() {
    const fs::path dir = fs::temp_directory_path() / "r573_media_encode_tests";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

std::vector<uint8_t> SyntheticFrame(int frame_index) {
    std::vector<uint8_t> bgra((size_t)kSrcW * kSrcH * 4);
    for (int y = 0; y < kSrcH; y++) {
        for (int x = 0; x < kSrcW; x++) {
            const size_t o = (((size_t)y * kSrcW) + x) * 4;
            bgra[o + 0] = (uint8_t)((x * 4 + frame_index * 16) & 0xFF);
            bgra[o + 1] = (uint8_t)((y * 5 + frame_index * 8) & 0xFF);
            bgra[o + 2] = (uint8_t)((x + y) & 0xFF);
            bgra[o + 3] = (uint8_t)(x < kSrcW / 2 ? 255 : 128);
        }
    }
    return bgra;
}

MediaSink::Params BaseParams(MediaSink::Format format, const std::string& name) {
    MediaSink::Params p;
    p.output_path = (OutDir() / name).string();
    p.format = format;
    p.src_width = kSrcW;
    p.src_height = kSrcH;
    p.fps = 30;
    p.quality = 30;
    p.keyframe_interval = 5;
    p.prefer_hardware = false;
    return p;
}

struct EncodeResult {
    bool opened = false;
    bool finished = false;
    std::string error;
    std::string path;
};

EncodeResult EncodeSynthetic(const MediaSink::Params& p) {
    EncodeResult r;
    MediaSink::Sink sink;
    r.opened = sink.Open(p);
    if (!r.opened) {
        r.error = sink.LastError();
        return r;
    }
    r.path = sink.OutputPath();
    for (int i = 0; i < kFrames; i++) {
        const std::vector<uint8_t> frame = SyntheticFrame(i);
        if (!sink.SubmitFrame(frame.data(), i)) {
            r.error = sink.LastError();
            return r;
        }
    }
    r.finished = sink.Finish();
    if (!r.finished) r.error = sink.LastError();
    return r;
}

struct ProbeResult {
    bool ok = false;
    int nb_streams = 0;
    int width = 0;
    int height = 0;
};

ProbeResult ProbeFile(const std::string& path) {
    ProbeResult pr;
    AVFormatContext* ctx = nullptr;
    if (avformat_open_input(&ctx, path.c_str(), nullptr, nullptr) != 0) return pr;
    if (avformat_find_stream_info(ctx, nullptr) >= 0) {
        pr.ok = true;
        pr.nb_streams = (int)ctx->nb_streams;
        const std::span<AVStream*> streams{ctx->streams, (size_t)ctx->nb_streams};
        for (const AVStream* stream : streams) {
            const AVCodecParameters* par = stream->codecpar;
            if (par->codec_type == AVMEDIA_TYPE_VIDEO && pr.width == 0) {
                pr.width = par->width;
                pr.height = par->height;
            }
        }
    }
    avformat_close_input(&ctx);
    return pr;
}

void CheckEncoded(const EncodeResult& r, int min_streams, int want_w, int want_h) {
    INFO("error: " << r.error);
    REQUIRE(r.opened);
    REQUIRE(r.finished);
    std::error_code ec;
    REQUIRE(fs::exists(r.path, ec));
    CHECK(fs::file_size(r.path, ec) > 0);
    const ProbeResult pr = ProbeFile(r.path);
    REQUIRE(pr.ok);
    CHECK(pr.nb_streams >= min_streams);
    if (want_w > 0) {
        CHECK(pr.width == want_w);
        CHECK(pr.height == want_h);
    }
}

}

TEST_CASE("AVIF encodes color plus alpha streams at source size") {
    const EncodeResult r = EncodeSynthetic(BaseParams(MediaSink::Format::AVIF, "encode_test.avif"));
    CheckEncoded(r, 2, kSrcW, kSrcH);
}

TEST_CASE("WebM VP9 encodes one stream and honors output scaling") {
    MediaSink::Params p = BaseParams(MediaSink::Format::WebM_VP9, "encode_test_vp9.webm");
    p.out_width = 32;
    p.out_height = 24;
    const EncodeResult r = EncodeSynthetic(p);
    CheckEncoded(r, 1, 32, 24);
}

TEST_CASE("WebM AV1 software path encodes one stream") {
    const EncodeResult r =
        EncodeSynthetic(BaseParams(MediaSink::Format::WebM_AV1, "encode_test_av1.webm"));
    CheckEncoded(r, 1, kSrcW, kSrcH);
}

TEST_CASE("WebP animation encodes one stream") {
    const EncodeResult r =
        EncodeSynthetic(BaseParams(MediaSink::Format::WebP_Anim, "encode_test.webp"));
    CheckEncoded(r, 1, 0, 0);
}

TEST_CASE("MP4 HEVC-alpha encodes one hvc1 stream") {
    const EncodeResult r =
        EncodeSynthetic(BaseParams(MediaSink::Format::MP4_HEVC_Alpha, "encode_test_hevc.mp4"));
    CheckEncoded(r, 1, kSrcW, kSrcH);
}

TEST_CASE("MP4 H264 without hardware reports the documented no-software-encoder error") {
    const EncodeResult r =
        EncodeSynthetic(BaseParams(MediaSink::Format::MP4_H264, "encode_test_h264.mp4"));
    if (r.opened) {
        SUCCEED("this ffmpeg build has a software H.264 encoder; encode path covered elsewhere");
        return;
    }
    CHECK(r.error.find("H.264") != std::string::npos);
}

TEST_CASE("PNG sequence writes numbered frames") {
    const fs::path dir = OutDir() / "png_seq";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    const MediaSink::Params p = BaseParams(MediaSink::Format::PNG_Sequence, "png_seq");
    const EncodeResult r = EncodeSynthetic(p);
    INFO("error: " << r.error);
    REQUIRE(r.opened);
    REQUIRE(r.finished);
    int pngs = 0;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (e.path().extension() == ".png") pngs++;
    }
    CHECK(pngs == kFrames);
}
