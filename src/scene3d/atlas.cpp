#include "scene3d/atlas.h"

#include "formats/gcz.h"
#include "formats/inz.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Scene3d {

namespace {

constexpr uint32_t kPackerFillColorKey = 0x0000FF00U;

uint32_t ArgbAt(const std::vector<uint8_t>& bgra, size_t texel) {
    return ((uint32_t)bgra[(texel * 4) + 3] << 24U) | ((uint32_t)bgra[(texel * 4) + 2] << 16U) |
           ((uint32_t)bgra[(texel * 4) + 1] << 8U) | (uint32_t)bgra[texel * 4];
}

void ApplyColorKey(std::vector<uint8_t>& bgra, uint32_t argb_key) {
    for (size_t texel = 0; (texel * 4) + 4 <= bgra.size(); texel++) {
        if (ArgbAt(bgra, texel) != argb_key) continue;
        for (size_t k = 0; k < 4; k++)
            bgra[(texel * 4) + k] = 0;
    }
}

}

void ScatterSlice(const Gcz::Tile& src, const Inz::AtlasGrid& grid, int atlas_x, int atlas_y,
                  std::vector<Tile>& tiles) {
    if (grid.tile_width <= 0 || grid.tile_height <= 0 || grid.tiles_per_row <= 0) return;
    std::vector<uint8_t> bgra;
    Gcz::ExpandToBgra(src, bgra);
    ApplyColorKey(bgra, kPackerFillColorKey);
    for (int row = 0; row < src.height; row++) {
        const int ay = atlas_y + row;
        const auto tile_row = (size_t)(ay / grid.tile_height);
        const int in_y = ay % grid.tile_height;
        for (int col = 0; col < src.width; col++) {
            const int ax = atlas_x + col;
            const size_t index =
                (tile_row * (size_t)grid.tiles_per_row) + (size_t)(ax / grid.tile_width);
            if (index >= tiles.size()) continue;
            const size_t dst =
                (((size_t)in_y * (size_t)grid.tile_width) + (size_t)(ax % grid.tile_width)) * 4;
            const size_t from = ((((size_t)row * (size_t)src.width) + (size_t)col)) * 4;
            std::copy_n(bgra.begin() + (ptrdiff_t)from, 4,
                        tiles[index].bgra.begin() + (ptrdiff_t)dst);
        }
    }
}

}
