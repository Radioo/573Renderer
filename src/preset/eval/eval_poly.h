#pragma once

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"

#include <array>
#include <string>
#include <vector>

namespace Preset::Eval {

using PolyVec2f = std::array<float, 2>;
using PolyVec3f = std::array<float, 3>;

struct PolyQuad {
    std::array<PolyVec3f, 4> corners = {};
    std::array<PolyVec2f, 4> uv = {};
};

struct PolyGridState {
    const Doc::Clip* from = nullptr;
    bool active = false;
    float alpha = 1.0F;
    float seconds = 0.0F;
    int movie_width = 0;
    int movie_height = 0;
    int texture_side = 0;
    std::string movie;
    std::vector<PolyQuad> quads;
};

PolyVec3f RotateAbout(const PolyVec3f& point, const PolyVec3f& pivot, float deg_x, float deg_y,
                      float deg_z);

std::vector<PolyVec2f> LatticePoints(const Doc::PolyTileGrid& grid);

std::vector<PolyQuad> PolyQuadsAt(const Doc::PolyTileGrid& grid,
                                  const std::vector<PolyVec2f>& lattice, int frame);

void ApplyPolyGrid(const Doc::Clip& clip, const Doc::PolyTileGrid& grid, int frame,
                   PolyGridState& out);

}
