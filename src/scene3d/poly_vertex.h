#pragma once

#include "scene3d/poly_grid.h"

#include <array>
#include <cstdint>

namespace Scene3d {

struct PolyVertex {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float nx = 0.0F;
    float ny = 1.0F;
    float nz = 0.0F;
    std::uint32_t diffuse = 0;
    float u = 0.0F;
    float v = 0.0F;
};

static_assert(sizeof(PolyVertex) == 36);

inline constexpr std::uint32_t kPolyFvf = 0x152U;

std::uint32_t DiffuseOf(float alpha);

std::array<PolyVertex, 4> StripOf(const PolyTile& tile, std::uint32_t diffuse);

}
