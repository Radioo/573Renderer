#include "media/media_format.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace MediaSink {

namespace {

struct Row {
    Format format;
    const char* token;
    const char* extension;
    bool is_dir;
    const char* label;
};

constexpr std::array<Row, kFormatCount> kTable = {{
    {.format = Format::AVIF,
     .token = "avif",
     .extension = ".avif",
     .is_dir = false,
     .label = "AVIF (image, alpha)"},
    {.format = Format::WebM_VP9,
     .token = "webm-vp9",
     .extension = ".webm",
     .is_dir = false,
     .label = "WebM VP9 (video, alpha, software)"},
    {.format = Format::WebM_AV1,
     .token = "webm-av1",
     .extension = ".webm",
     .is_dir = false,
     .label = "WebM AV1 (video, opaque, NVENC)"},
    {.format = Format::WebP_Anim,
     .token = "webp",
     .extension = ".webp",
     .is_dir = false,
     .label = "WebP (image, alpha, software) - recommended"},
    {.format = Format::PNG_Sequence,
     .token = "png",
     .extension = "",
     .is_dir = true,
     .label = "PNG sequence (folder of frames, lossless)"},
    {.format = Format::MP4_H264,
     .token = "mp4",
     .extension = ".mp4",
     .is_dir = false,
     .label = "MP4 H.264 (video, opaque, most compatible)"},
    {.format = Format::MP4_HEVC_Alpha,
     .token = "mp4-hevc-alpha",
     .extension = ".mp4",
     .is_dir = false,
     .label = "MP4 HEVC alpha (video, alpha, Safari)"},
}};

struct Alias {
    std::string_view token;
    Format format;
};

constexpr std::array<Alias, 12> kAliases = {{
    {.token = "webm", .format = Format::WebM_VP9},
    {.token = "vp9", .format = Format::WebM_VP9},
    {.token = "av1", .format = Format::WebM_AV1},
    {.token = "webp-anim", .format = Format::WebP_Anim},
    {.token = "png-seq", .format = Format::PNG_Sequence},
    {.token = "pngseq", .format = Format::PNG_Sequence},
    {.token = "h264", .format = Format::MP4_H264},
    {.token = "avc", .format = Format::MP4_H264},
    {.token = "mp4-h264", .format = Format::MP4_H264},
    {.token = "hevc", .format = Format::MP4_HEVC_Alpha},
    {.token = "hevc-alpha", .format = Format::MP4_HEVC_Alpha},
    {.token = "mp4-hevc", .format = Format::MP4_HEVC_Alpha},
}};

const Row& RowOf(Format f) {
    auto i = static_cast<std::size_t>(f);
    if (i >= kTable.size()) i = 0;
    return kTable.at(i);
}

}

const char* FormatLabel(Format f) {
    return RowOf(f).label;
}

const char* FormatToken(Format f) {
    return RowOf(f).token;
}

const char* FormatExtension(Format f) {
    return RowOf(f).extension;
}

bool WritesDirectory(Format f) {
    return RowOf(f).is_dir;
}

bool UsesKeyframeInterval(Format f) {
    return f != Format::PNG_Sequence && f != Format::WebP_Anim;
}

int ToIndex(Format f) {
    return static_cast<int>(f);
}

Format FromIndex(int i) {
    if (i < 0 || i >= kFormatCount) return Format::AVIF;
    return static_cast<Format>(i);
}

bool ParseToken(const char* s, Format* out) {
    if (s == nullptr || out == nullptr) return false;
    const std::string_view token(s);
    for (const Row& row : kTable) {
        if (token == row.token) {
            *out = row.format;
            return true;
        }
    }
    for (const Alias& alias : kAliases) {
        if (token == alias.token) {
            *out = alias.format;
            return true;
        }
    }
    if (token == "safari") {
        *out = Format::MP4_HEVC_Alpha;
        return true;
    }
    return false;
}

std::string MakeOutputPath(const std::string& stem, Format f) {
    if (WritesDirectory(f)) {
        if (!stem.empty() && (stem.back() == '\\' || stem.back() == '/')) {
            return stem.substr(0, stem.size() - 1);
        }
        return stem;
    }
    return stem + FormatExtension(f);
}

Format HardwareProbeFormat(Format f) {
    return f == Format::MP4_H264 ? Format::MP4_H264 : Format::AVIF;
}

std::string DeriveExportStem(const std::string& active_ifs, const std::string& playing_animation) {
    if (active_ifs.empty()) return "export";
    std::string stem = active_ifs;
    const std::size_t dot = stem.find_last_of('.');
    if (dot != std::string::npos) stem.resize(dot);
    if (!playing_animation.empty()) stem += "_" + playing_animation;
    return stem;
}

}
