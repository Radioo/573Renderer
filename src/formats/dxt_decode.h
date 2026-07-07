#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace Dxt {

constexpr uint32_t kFmtDxt1 = 0x17;
constexpr uint32_t kFmtDxt2 = 0x18;
constexpr uint32_t kFmtDxt3 = 0x19;
constexpr uint32_t kFmtDxt4 = 0x1A;
constexpr uint32_t kFmtDxt5 = 0x1B;

[[nodiscard]] constexpr bool IsDxtFormat(uint32_t fmt) {
    return fmt >= kFmtDxt1 && fmt <= kFmtDxt5;
}

[[nodiscard]] constexpr std::size_t BlockSize(uint32_t fmt) {
    return fmt == kFmtDxt1 ? 8 : 16;
}

[[nodiscard]] constexpr std::size_t EncodedSize(uint32_t fmt, int width, int height) {
    const std::size_t blocks_x = (static_cast<std::size_t>(width) + 3) / 4;
    const std::size_t blocks_y = (static_cast<std::size_t>(height) + 3) / 4;
    return blocks_x * blocks_y * BlockSize(fmt);
}

void Decompress(uint32_t fmt, int src_w, int src_h, std::span<const uint8_t> src,
                std::span<uint8_t> dst, int dst_pitch);

}
