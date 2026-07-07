#pragma once

#include <cstdint>
#include <span>

struct AfpVertex {
    float x;
    float y;
    float z;
    uint32_t color;
    float u;
    float v;
};
static_assert(sizeof(AfpVertex) == 24);

namespace Render {

[[nodiscard]] float SnapHalfPixel(float v);

int DecodeAfpVertices(std::span<const float> src, std::span<const float> uv_data, int flags,
                      std::span<AfpVertex> out);

void InsetQuadUvs(std::span<AfpVertex> verts, int prim_type, int tex_w, int tex_h);

[[nodiscard]] int AfpPrimCount(int prim_type, int vert_count);

}
