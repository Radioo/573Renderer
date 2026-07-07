#include "formats/bgra_crop.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Bgra {

std::vector<uint8_t> Crop(std::span<const uint8_t> atlas, int aw, int ah, int& x, int& y, int& w,
                          int& h) {
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > aw) w = aw - x;
    if (y + h > ah) h = ah - y;
    if (w <= 0 || h <= 0) {
        w = h = 0;
        return {};
    }
    std::vector<uint8_t> out(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4);
    const auto row_bytes = static_cast<std::size_t>(w) * 4;
    for (int row = 0; row < h; ++row) {
        const auto src_off = ((static_cast<std::size_t>(y + row) * static_cast<std::size_t>(aw)) +
                              static_cast<std::size_t>(x)) *
                             4;
        const auto dst_off = static_cast<std::size_t>(row) * row_bytes;
        std::copy_n(atlas.begin() + static_cast<std::ptrdiff_t>(src_off), row_bytes,
                    out.begin() + static_cast<std::ptrdiff_t>(dst_off));
    }
    return out;
}

}
