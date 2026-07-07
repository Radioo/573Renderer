#include "render/vertex_math.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace Render {

float SnapHalfPixel(float v) {
    return std::floor(v + 0.5F) - 0.5F;
}

int DecodeAfpVertices(std::span<const float> src, std::span<const float> uv_data, int flags,
                      std::span<AfpVertex> out) {
    std::size_t si = 0;
    for (std::size_t i = 0; i < out.size(); i++) {
        float u = uv_data[(i * 2) + 0];
        float v = uv_data[(i * 2) + 1];
        uint32_t color = 0xFFFFFFFF;
        float x = 0;
        float y = 0;
        float z = 0;

        if ((flags & 0x08) != 0) {
            u = src[si++];
            v = src[si++];
        }
        if ((flags & 0x10) != 0) {
            si += 2;
        }
        if ((flags & 0x04) != 0) {
            std::memcpy(&color, &src[si], sizeof(color));
            si++;
        }
        if ((flags & 0x02) != 0) {
            x = src[si++];
            y = src[si++];
            z = src[si++];
        } else if ((flags & 0x01) != 0) {
            x = src[si++];
            y = src[si++];
        }

        out[i] = {
            .x = SnapHalfPixel(x), .y = SnapHalfPixel(y), .z = z, .color = color, .u = u, .v = v};
    }
    return static_cast<int>(si);
}

namespace {

void InsetOneQuad(std::span<AfpVertex> quad, float inset_u, float inset_v, int tex_w, int tex_h) {
    float umin = quad[0].u;
    float umax = quad[0].u;
    float vmin = quad[0].v;
    float vmax = quad[0].v;
    for (std::size_t i = 1; i < quad.size(); i++) {
        umin = std::min(umin, quad[i].u);
        umax = std::max(umax, quad[i].u);
        vmin = std::min(vmin, quad[i].v);
        vmax = std::max(vmax, quad[i].v);
    }
    const float ucen = (umin + umax) * 0.5F;
    const float vcen = (vmin + vmax) * 0.5F;
    const bool do_u = (umax - umin) * static_cast<float>(tex_w) > 1.0F;
    const bool do_v = (vmax - vmin) * static_cast<float>(tex_h) > 1.0F;
    for (AfpVertex& vert : quad) {
        if (do_u) {
            if (vert.u < ucen) {
                vert.u += inset_u;
            } else if (vert.u > ucen) {
                vert.u -= inset_u;
            }
        }
        if (do_v) {
            if (vert.v < vcen) {
                vert.v += inset_v;
            } else if (vert.v > vcen) {
                vert.v -= inset_v;
            }
        }
    }
}

}

void InsetQuadUvs(std::span<AfpVertex> verts, int prim_type, int tex_w, int tex_h) {
    if (prim_type != 4 || (verts.size() % 6) != 0 || tex_w <= 0 || tex_h <= 0) return;
    const float inset_u = 0.25F / static_cast<float>(tex_w);
    const float inset_v = 0.25F / static_cast<float>(tex_h);
    for (std::size_t base = 0; base < verts.size(); base += 6) {
        InsetOneQuad(verts.subspan(base, 6), inset_u, inset_v, tex_w, tex_h);
    }
}

int AfpPrimCount(int prim_type, int vert_count) {
    switch (prim_type) {
    case 1:
    case 3:
        return vert_count / 2;
    case 2:
        return vert_count - 1;
    case 4:
        return vert_count / 3;
    case 5:
    case 6:
    default:
        return vert_count - 2;
    }
}

}
