#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Gcz {

struct Tile {
    int width = 0;
    int height = 0;
    int origin_x = 0;
    int origin_y = 0;
    int bits = 16;
    std::vector<uint8_t> pixels;
};

bool Parse(std::span<const uint8_t> payload, Tile& out, std::string& err);

void ExpandToBgra(const Tile& tile, std::vector<uint8_t>& out);

}
