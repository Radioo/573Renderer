#include "scene3d/poly_vertex.h"

#include "scene3d/poly_grid.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace Scene3d {

std::uint32_t DiffuseOf(float alpha) {
    const auto level = (std::uint32_t)std::clamp(alpha * 255.0F, 0.0F, 255.0F);
    return (level << 24U) | 0x00FFFFFFU;
}

std::array<PolyVertex, 4> StripOf(const PolyTile& tile, std::uint32_t diffuse) {
    std::array<PolyVertex, 4> strip = {};
    for (std::size_t i = 0; i < strip.size(); i++) {
        strip[i].x = tile.corners[i][0];
        strip[i].y = tile.corners[i][1];
        strip[i].z = tile.corners[i][2];
        strip[i].nx = 0.0F;
        strip[i].ny = 1.0F;
        strip[i].nz = 0.0F;
        strip[i].diffuse = diffuse;
        strip[i].u = tile.uv[i][0];
        strip[i].v = tile.uv[i][1];
    }
    return strip;
}

}
