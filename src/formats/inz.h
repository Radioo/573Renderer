#pragma once

#include <string>
#include <vector>

namespace Inz {

struct ImageSlice {
    std::string name;
    int flag = 0;
};

struct Pattern {
    std::string name;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct Manifest {
    std::vector<ImageSlice> slices;
    std::vector<Pattern> patterns;
};

struct AtlasGrid {
    int tile_width;
    int tile_height;
    int tiles_per_row;
};

struct Region {
    int tile = -1;
    float u_scale = 1.0F;
    float u_bias = 0.0F;
    float v_scale = 1.0F;
    float v_bias = 0.0F;
};

bool Parse(const std::string& text, Manifest& out, std::string& err);

Region ResolveRegion(const Manifest& manifest, const std::string& texture_name,
                     const AtlasGrid& grid);

}
