#include "formats/dxt_decode.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Dxt {

namespace {

constexpr std::size_t kBlockPixels = 16;
constexpr std::size_t kBgraBytes = 4;

void Rgb565ToBgr8(uint16_t c, uint8_t& b, uint8_t& g, uint8_t& r) {
    const auto r5 = static_cast<uint8_t>((c >> 11U) & 0x1FU);
    const auto g6 = static_cast<uint8_t>((c >> 5U) & 0x3FU);
    const auto b5 = static_cast<uint8_t>(c & 0x1FU);
    r = static_cast<uint8_t>((r5 << 3U) | (r5 >> 2U));
    g = static_cast<uint8_t>((g6 << 2U) | (g6 >> 4U));
    b = static_cast<uint8_t>((b5 << 3U) | (b5 >> 2U));
}

void DecodeBc1Color(std::span<const uint8_t> block, std::span<uint8_t> pixels, bool allow_punch) {
    const auto c0 = static_cast<uint16_t>(block[0] | (static_cast<uint16_t>(block[1]) << 8U));
    const auto c1 = static_cast<uint16_t>(block[2] | (static_cast<uint16_t>(block[3]) << 8U));
    std::array<uint8_t, 4> pal_b{};
    std::array<uint8_t, 4> pal_g{};
    std::array<uint8_t, 4> pal_r{};
    std::array<uint8_t, 4> pal_a{};
    Rgb565ToBgr8(c0, pal_b.at(0), pal_g.at(0), pal_r.at(0));
    Rgb565ToBgr8(c1, pal_b.at(1), pal_g.at(1), pal_r.at(1));
    pal_a.at(0) = 0xFF;
    pal_a.at(1) = 0xFF;
    if (c0 > c1 || !allow_punch) {
        pal_b.at(2) = static_cast<uint8_t>(((2 * pal_b.at(0)) + pal_b.at(1)) / 3);
        pal_g.at(2) = static_cast<uint8_t>(((2 * pal_g.at(0)) + pal_g.at(1)) / 3);
        pal_r.at(2) = static_cast<uint8_t>(((2 * pal_r.at(0)) + pal_r.at(1)) / 3);
        pal_a.at(2) = 0xFF;
        pal_b.at(3) = static_cast<uint8_t>((pal_b.at(0) + (2 * pal_b.at(1))) / 3);
        pal_g.at(3) = static_cast<uint8_t>((pal_g.at(0) + (2 * pal_g.at(1))) / 3);
        pal_r.at(3) = static_cast<uint8_t>((pal_r.at(0) + (2 * pal_r.at(1))) / 3);
        pal_a.at(3) = 0xFF;
    } else {
        pal_b.at(2) = static_cast<uint8_t>((pal_b.at(0) + pal_b.at(1)) / 2);
        pal_g.at(2) = static_cast<uint8_t>((pal_g.at(0) + pal_g.at(1)) / 2);
        pal_r.at(2) = static_cast<uint8_t>((pal_r.at(0) + pal_r.at(1)) / 2);
        pal_a.at(2) = 0xFF;
        pal_b.at(3) = 0;
        pal_g.at(3) = 0;
        pal_r.at(3) = 0;
        pal_a.at(3) = 0;
    }
    const uint32_t indices =
        static_cast<uint32_t>(block[4]) | (static_cast<uint32_t>(block[5]) << 8U) |
        (static_cast<uint32_t>(block[6]) << 16U) | (static_cast<uint32_t>(block[7]) << 24U);
    for (std::size_t i = 0; i < kBlockPixels; i++) {
        const std::size_t idx = (indices >> (i * 2)) & 0x3U;
        pixels[(i * kBgraBytes) + 0] = pal_b.at(idx);
        pixels[(i * kBgraBytes) + 1] = pal_g.at(idx);
        pixels[(i * kBgraBytes) + 2] = pal_r.at(idx);
        pixels[(i * kBgraBytes) + 3] = pal_a.at(idx);
    }
}

void OverwriteAlphaBc2(std::span<const uint8_t> alpha_src, std::span<uint8_t> pixels) {
    for (std::size_t i = 0; i < 8; i++) {
        const uint8_t b = alpha_src[i];
        const auto a0 = static_cast<uint8_t>((b & 0x0FU) * 17);
        const auto a1 = static_cast<uint8_t>((b >> 4U) * 17);
        pixels[(((i * 2) + 0) * kBgraBytes) + 3] = a0;
        pixels[(((i * 2) + 1) * kBgraBytes) + 3] = a1;
    }
}

void OverwriteAlphaBc3(std::span<const uint8_t> alpha_src, std::span<uint8_t> pixels) {
    const uint8_t a0 = alpha_src[0];
    const uint8_t a1 = alpha_src[1];
    std::array<uint8_t, 8> pal{};
    pal.at(0) = a0;
    pal.at(1) = a1;
    if (a0 > a1) {
        for (std::size_t i = 1; i <= 6; i++) {
            pal.at(i + 1) = static_cast<uint8_t>((((7 - i) * a0) + (i * a1)) / 7);
        }
    } else {
        for (std::size_t i = 1; i <= 4; i++) {
            pal.at(i + 1) = static_cast<uint8_t>((((5 - i) * a0) + (i * a1)) / 5);
        }
        pal.at(6) = 0x00;
        pal.at(7) = 0xFF;
    }
    uint64_t bits = 0;
    for (std::size_t i = 0; i < 6; i++) {
        bits |= static_cast<uint64_t>(alpha_src[2 + i]) << (i * 8);
    }
    for (std::size_t i = 0; i < kBlockPixels; i++) {
        const std::size_t idx = (bits >> (i * 3)) & 0x7U;
        pixels[(i * kBgraBytes) + 3] = pal.at(idx);
    }
}

}

void Decompress(uint32_t fmt, int src_w, int src_h, std::span<const uint8_t> src,
                std::span<uint8_t> dst, int dst_pitch) {
    if (src_w <= 0 || src_h <= 0 || dst_pitch <= 0) return;
    const std::size_t block_size = BlockSize(fmt);
    const std::size_t blocks_x = (static_cast<std::size_t>(src_w) + 3) / 4;
    const std::size_t blocks_y = (static_cast<std::size_t>(src_h) + 3) / 4;
    const auto width = static_cast<std::size_t>(src_w);
    const auto height = static_cast<std::size_t>(src_h);
    const auto pitch = static_cast<std::size_t>(dst_pitch);
    std::array<uint8_t, kBlockPixels * kBgraBytes> pixels{};

    for (std::size_t by = 0; by < blocks_y; by++) {
        for (std::size_t bx = 0; bx < blocks_x; bx++) {
            const std::size_t src_off = ((by * blocks_x) + bx) * block_size;
            if (src_off + block_size > src.size()) return;
            const std::span<const uint8_t> block = src.subspan(src_off, block_size);
            const std::span<const uint8_t> color = (block_size == 16) ? block.subspan(8, 8) : block;
            DecodeBc1Color(color, pixels, fmt == kFmtDxt1);
            switch (fmt) {
            case kFmtDxt2:
            case kFmtDxt3:
                OverwriteAlphaBc2(block, pixels);
                break;
            case kFmtDxt4:
            case kFmtDxt5:
                OverwriteAlphaBc3(block, pixels);
                break;
            default:
                break;
            }
            for (std::size_t yi = 0; yi < 4; yi++) {
                const std::size_t dy = (by * 4) + yi;
                if (dy >= height) break;
                const std::size_t xn = std::min<std::size_t>(4, width - (bx * 4));
                const std::size_t dst_off = (dy * pitch) + (bx * 4 * kBgraBytes);
                if (dst_off + (xn * kBgraBytes) > dst.size()) return;
                std::copy_n(pixels.begin() + static_cast<std::ptrdiff_t>(yi * 4 * kBgraBytes),
                            xn * kBgraBytes, dst.begin() + static_cast<std::ptrdiff_t>(dst_off));
            }
        }
    }
}

}
