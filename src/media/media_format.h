#pragma once

#include <cstdint>
#include <string>

namespace MediaSink {

enum class Format : std::uint8_t {
    AVIF = 0,
    WebM_VP9 = 1,
    WebM_AV1 = 2,
    WebP_Anim = 3,
    PNG_Sequence = 4,
    MP4_H264 = 5,
    MP4_HEVC_Alpha = 6,
};
constexpr int kFormatCount = 7;

[[nodiscard]] const char* FormatLabel(Format f);
[[nodiscard]] const char* FormatToken(Format f);
[[nodiscard]] const char* FormatExtension(Format f);
[[nodiscard]] bool WritesDirectory(Format f);
[[nodiscard]] bool UsesKeyframeInterval(Format f);
[[nodiscard]] bool ParseToken(const char* s, Format* out);
[[nodiscard]] Format FromIndex(int i);
[[nodiscard]] int ToIndex(Format f);
[[nodiscard]] std::string MakeOutputPath(const std::string& stem, Format f);

[[nodiscard]] Format HardwareProbeFormat(Format f);

[[nodiscard]] std::string DeriveExportStem(const std::string& active_ifs,
                                           const std::string& playing_animation);

}
