#include <catch2/catch_test_macros.hpp>

#include "media/media_format.h"

#include <string>

namespace {
using MediaSink::Format;
}

TEST_CASE("every format round-trips token and index") {
    for (int i = 0; i < MediaSink::kFormatCount; i++) {
        const Format f = MediaSink::FromIndex(i);
        CHECK(MediaSink::ToIndex(f) == i);
        Format parsed = Format::AVIF;
        REQUIRE(MediaSink::ParseToken(MediaSink::FormatToken(f), &parsed));
        CHECK(parsed == f);
        CHECK(MediaSink::FormatLabel(f) != std::string{});
    }
}

TEST_CASE("FromIndex clamps out-of-range to AVIF") {
    CHECK(MediaSink::FromIndex(-1) == Format::AVIF);
    CHECK(MediaSink::FromIndex(MediaSink::kFormatCount) == Format::AVIF);
}

TEST_CASE("ParseToken accepts the legacy aliases") {
    Format f = Format::AVIF;
    REQUIRE(MediaSink::ParseToken("webm", &f));
    CHECK(f == Format::WebM_VP9);
    REQUIRE(MediaSink::ParseToken("av1", &f));
    CHECK(f == Format::WebM_AV1);
    REQUIRE(MediaSink::ParseToken("pngseq", &f));
    CHECK(f == Format::PNG_Sequence);
    REQUIRE(MediaSink::ParseToken("h264", &f));
    CHECK(f == Format::MP4_H264);
    REQUIRE(MediaSink::ParseToken("safari", &f));
    CHECK(f == Format::MP4_HEVC_Alpha);
}

TEST_CASE("ParseToken rejects junk and null") {
    Format f = Format::AVIF;
    CHECK_FALSE(MediaSink::ParseToken("gif", &f));
    CHECK_FALSE(MediaSink::ParseToken(nullptr, &f));
    CHECK_FALSE(MediaSink::ParseToken("avif", nullptr));
}

TEST_CASE("extensions and directory outputs are consistent") {
    CHECK(MediaSink::FormatExtension(Format::AVIF) == std::string{".avif"});
    CHECK(MediaSink::FormatExtension(Format::PNG_Sequence) == std::string{});
    CHECK(MediaSink::WritesDirectory(Format::PNG_Sequence));
    CHECK_FALSE(MediaSink::WritesDirectory(Format::MP4_H264));
}

TEST_CASE("keyframe interval applies to video codecs only") {
    CHECK(MediaSink::UsesKeyframeInterval(Format::AVIF));
    CHECK(MediaSink::UsesKeyframeInterval(Format::WebM_VP9));
    CHECK(MediaSink::UsesKeyframeInterval(Format::WebM_AV1));
    CHECK(MediaSink::UsesKeyframeInterval(Format::MP4_H264));
    CHECK(MediaSink::UsesKeyframeInterval(Format::MP4_HEVC_Alpha));
    CHECK_FALSE(MediaSink::UsesKeyframeInterval(Format::PNG_Sequence));
    CHECK_FALSE(MediaSink::UsesKeyframeInterval(Format::WebP_Anim));
}

TEST_CASE("MakeOutputPath appends extension or trims directory slash") {
    CHECK(MediaSink::MakeOutputPath("out/clip", Format::AVIF) == "out/clip.avif");
    CHECK(MediaSink::MakeOutputPath("out/frames/", Format::PNG_Sequence) == "out/frames");
    CHECK(MediaSink::MakeOutputPath("out/frames", Format::PNG_Sequence) == "out/frames");
}

TEST_CASE("HardwareProbeFormat routes H264 to h264_nvenc and the rest to the AV1 probe") {
    CHECK(MediaSink::HardwareProbeFormat(Format::MP4_H264) == Format::MP4_H264);
    CHECK(MediaSink::HardwareProbeFormat(Format::AVIF) == Format::AVIF);
    CHECK(MediaSink::HardwareProbeFormat(Format::WebM_AV1) == Format::AVIF);
    CHECK(MediaSink::HardwareProbeFormat(Format::WebP_Anim) == Format::AVIF);
    CHECK(MediaSink::HardwareProbeFormat(Format::MP4_HEVC_Alpha) == Format::AVIF);
}

TEST_CASE("DeriveExportStem strips the extension and appends the animation") {
    CHECK(MediaSink::DeriveExportStem("select_bg_vi.ifs", "bg_common") == "select_bg_vi_bg_common");
    CHECK(MediaSink::DeriveExportStem("title.ifs", "") == "title");
    CHECK(MediaSink::DeriveExportStem("noext", "anim") == "noext_anim");
    CHECK(MediaSink::DeriveExportStem("", "anim") == "export");
    CHECK(MediaSink::DeriveExportStem("", "") == "export");
}
