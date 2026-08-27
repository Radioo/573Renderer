#include "preset/eval/eval_poly.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/preset_rng.h"
#include "support/math/float_trig.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <string>
#include <vector>

namespace Preset::Eval {

namespace {

constexpr float kDegreesToRadians = std::bit_cast<float>(0x3C8EFA35U);

int Sides(int count) {
    return std::clamp(count, 1, 16);
}

float Parity(int value, int modulus) {
    return (float)(((value % modulus) + modulus) % modulus);
}

}

PolyVec3f RotateAbout(const PolyVec3f& point, const PolyVec3f& pivot, float deg_x, float deg_y,
                      float deg_z) {
    const float dx = point[0] - pivot[0];
    const float dy = point[1] - pivot[1];
    const float dz = point[2] - pivot[2];

    const float sin_x = Support::Sinf(deg_x * kDegreesToRadians);
    const float cos_x = Support::Cosf(deg_x * kDegreesToRadians);
    const float z1 = (dz * cos_x) - (dy * sin_x);
    const float y1 = (dy * cos_x) + (dz * sin_x);

    const float sin_y = Support::Sinf(deg_y * kDegreesToRadians);
    const float cos_y = Support::Cosf(deg_y * kDegreesToRadians);
    const float x2 = (dx * cos_y) - (z1 * sin_y);
    const float z2 = (z1 * cos_y) + (dx * sin_y);

    const float sin_z = Support::Sinf(deg_z * kDegreesToRadians);
    const float cos_z = Support::Cosf(deg_z * kDegreesToRadians);
    return PolyVec3f{(x2 * cos_z) + (y1 * sin_z) + pivot[0], (y1 * cos_z) - (x2 * sin_z) + pivot[1],
                     z2 + pivot[2]};
}

std::vector<PolyVec2f> LatticePoints(const Doc::PolyTileGrid& grid) {
    const int rows = Sides(grid.rows);
    const int cols = Sides(grid.cols);
    const auto amplitude = (float)grid.lattice_amplitude;
    Preset::CrtRand rng;
    rng.Seed(grid.lattice_seed);

    std::vector<PolyVec2f> out;
    out.reserve((std::size_t)(rows + 1) * (std::size_t)(cols + 1));
    for (int row = 0; row <= rows; row++) {
        for (int col = 0; col <= cols; col++) {
            float y = (float)row / (float)rows;
            float x = (float)col / (float)cols;
            if (row != 0 && row != rows) y += ((float)(rng.Next() % 2) - 0.5F) * amplitude;
            if (col != 0 && col != cols) x += ((float)(rng.Next() % 2) - 0.5F) * amplitude;
            out.push_back(PolyVec2f{x, y});
        }
    }
    return out;
}

std::vector<PolyQuad> PolyQuadsAt(const Doc::PolyTileGrid& grid,
                                  const std::vector<PolyVec2f>& lattice, int frame) {
    const int rows = Sides(grid.rows);
    const int cols = Sides(grid.cols);
    const int stride = cols + 1;
    if (lattice.size() < (std::size_t)(rows + 1) * (std::size_t)stride) return {};

    const auto f = (float)frame;
    const auto depth = (float)grid.depth;
    const PolyVec3f orbit_pivot = {0.0F, 0.0F, depth};
    const float inv_u = 1.0F / (2.0F * (float)cols);
    const float inv_v = 1.0F / (2.0F * (float)rows);
    const auto scale_u = (float)(grid.movie_size[0] / grid.texture_size);
    const auto scale_v = (float)(grid.movie_size[1] / grid.texture_size);
    const float burst = (frame >= grid.burst_from)
                            ? ((float)(frame - grid.burst_from + 1) * (float)grid.burst_step)
                            : 0.0F;

    std::vector<PolyQuad> out;
    out.reserve((std::size_t)rows * (std::size_t)cols);
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            const int index = (row * cols) + col;
            const PolyVec3f centre = {
                ((float)col - ((float)(cols - 1) * 0.5F)) * (float)grid.spacing[0],
                ((float)row - ((float)(rows - 1) * 0.5F)) * (float)grid.spacing[1], depth};
            const float cell_u = ((2.0F * (float)col) + 1.0F) * inv_u;
            const float cell_v = ((2.0F * (float)row) + 1.0F) * inv_v;
            const std::array<int, 4> corner_of = {(stride * (row + 1)) + col,
                                                  (stride * (row + 1)) + col + 1,
                                                  (stride * row) + col, (stride * row) + col + 1};

            const float spin_x = Parity(col, 2) * f * (float)grid.spin_rates[0];
            const float spin_y = (Parity(row + 1, 2) - 0.5F) * f * (float)grid.spin_rates[1];
            const float spin_z = Parity(row + col, 2) * f * (float)grid.spin_rates[2];
            const float orbit_x = f * (float)grid.orbit_rates[0];
            const float orbit_y = (Parity(row, 2) - 0.5F) * f * (float)grid.orbit_rates[1];
            const float orbit_z = (Parity(row + col, 3) - 1.0F) * f * (float)grid.orbit_rates[2];
            const float lift =
                std::max(0.0F, burst - ((float)index * (float)grid.burst_delay_per_tile));

            PolyQuad quad;
            for (std::size_t k = 0; k < corner_of.size(); k++) {
                const PolyVec2f& point = lattice[(std::size_t)corner_of[k]];
                PolyVec3f corner = {centre[0] + ((point[0] - cell_u) * (float)grid.quad_scale[0]),
                                    centre[1] + ((point[1] - cell_v) * (float)grid.quad_scale[1]),
                                    centre[2]};
                corner = RotateAbout(corner, centre, spin_x, spin_y, spin_z);
                corner = RotateAbout(corner, orbit_pivot, orbit_x, orbit_y, orbit_z);
                corner[2] += lift;
                quad.corners[k] = corner;
                quad.uv[k] = PolyVec2f{point[0] * scale_u, point[1] * scale_v};
            }
            out.push_back(quad);
        }
    }
    return out;
}

void ApplyPolyGrid(const Doc::Clip& clip, const Doc::PolyTileGrid& grid, int frame,
                   PolyGridState& out) {
    out.from = &clip;
    out.active = true;
    out.alpha = (float)grid.alpha;
    out.movie = grid.texture.has_value() ? grid.texture->path : std::string();
    out.movie_width = (int)grid.movie_size[0];
    out.movie_height = (int)grid.movie_size[1];
    out.texture_side = (int)grid.texture_size;
    out.quads = PolyQuadsAt(grid, LatticePoints(grid), frame);
}

}
