#pragma once

#include "formats/gcz.h"
#include "formats/inz.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Scene3d {

struct Tile {
    std::string name;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> bgra;
};

void ScatterSlice(const Gcz::Tile& src, const Inz::AtlasGrid& grid, int atlas_x, int atlas_y,
                  std::vector<Tile>& tiles);

}
