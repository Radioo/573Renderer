#pragma once

#include "formats/sysidx.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Gc2d {

struct TileImage {
    int width = 0;
    int height = 0;
    int origin_x = 0;
    int origin_y = 0;
    std::vector<uint8_t> bgra;
};

struct Package {
    std::string name;
    SysIdx::Package index;
    std::vector<TileImage> tiles;
    std::vector<std::string> animation_names;
};

bool IsPackageDir(const std::string& dir);

bool Load(const std::string& dir, Package& out, std::string& err);

}
