#include "formats/frame_process.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Frame {

void CompositeOverOpaqueBg(std::span<uint8_t> bgra, float r, float g, float b) {
    const int bg_b_i = static_cast<int>(std::lround(b * 255.0F));
    const int bg_g_i = static_cast<int>(std::lround(g * 255.0F));
    const int bg_r_i = static_cast<int>(std::lround(r * 255.0F));
    const std::size_t n = bgra.size() / 4;
    for (std::size_t i = 0; i < n; i++) {
        const std::size_t o = i * 4;
        const int inv = 255 - bgra[o + 3];
        const int b2 = bgra[o + 0] + (((bg_b_i * inv) + 127) / 255);
        const int g2 = bgra[o + 1] + (((bg_g_i * inv) + 127) / 255);
        const int r2 = bgra[o + 2] + (((bg_r_i * inv) + 127) / 255);
        bgra[o + 0] = static_cast<uint8_t>(std::min(b2, 255));
        bgra[o + 1] = static_cast<uint8_t>(std::min(g2, 255));
        bgra[o + 2] = static_cast<uint8_t>(std::min(r2, 255));
        bgra[o + 3] = 255;
    }
}

CropSpec ClampCropToImage(CropSpec c, int img_w, int img_h) {
    CropSpec out;
    out.x = std::max(0, std::min(c.x, img_w - 1));
    out.y = std::max(0, std::min(c.y, img_h - 1));
    out.w = std::max(1, std::min(c.w, img_w - out.x));
    out.h = std::max(1, std::min(c.h, img_h - out.y));
    return out;
}

void CopyCropRegion(std::span<const uint8_t> bgra, int img_w, CropSpec c,
                    std::vector<uint8_t>& out) {
    const std::size_t row_bytes = static_cast<std::size_t>(c.w) * 4;
    out.resize(static_cast<std::size_t>(c.h) * row_bytes);
    for (int row = 0; row < c.h; row++) {
        const auto src_off =
            ((static_cast<std::size_t>(c.y + row) * static_cast<std::size_t>(img_w)) +
             static_cast<std::size_t>(c.x)) *
            4;
        const auto dst_off = static_cast<std::size_t>(row) * row_bytes;
        std::copy_n(bgra.begin() + static_cast<std::ptrdiff_t>(src_off), row_bytes,
                    out.begin() + static_cast<std::ptrdiff_t>(dst_off));
    }
}

}
