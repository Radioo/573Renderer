#include "render/command_trace.h"

#include <algorithm>
#include <format>
#include <span>
#include <string>

#include "render/hsv_filter.h"
#include "render/vertex_math.h"

namespace Render {

namespace {

const char* PsTag(bool hsl, bool add) {
    if (hsl) return "hsl";
    if (add) return "add";
    return "-";
}

}

std::string TraceSetLayer(unsigned int blend_mode, bool has_desc, const HsvDescriptor& d) {
    if (!has_desc) return std::format("layer mode={} hsv=-\n", blend_mode);
    return std::format("layer mode={} hsv={{id={} valid={} h={:.2f} s={:.2f} v={:.2f}}}\n",
                       blend_mode, static_cast<unsigned>(d.id), static_cast<unsigned>(d.valid),
                       d.hue, d.sat, d.val);
}

std::string TraceSetBlend(unsigned int mode, unsigned int flags) {
    return std::format("blend mode={} flags={}\n", mode, flags);
}

std::string TraceDraw(int prim_type, int tex_slot, bool hsl, bool add,
                      std::span<const AfpVertex> verts) {
    if (verts.empty()) return std::format("draw prim={} empty\n", prim_type);
    float xmin = verts[0].x;
    float xmax = verts[0].x;
    float ymin = verts[0].y;
    float ymax = verts[0].y;
    float umin = verts[0].u;
    float umax = verts[0].u;
    float vmin = verts[0].v;
    float vmax = verts[0].v;
    for (const AfpVertex& vert : verts) {
        xmin = std::min(xmin, vert.x);
        xmax = std::max(xmax, vert.x);
        ymin = std::min(ymin, vert.y);
        ymax = std::max(ymax, vert.y);
        umin = std::min(umin, vert.u);
        umax = std::max(umax, vert.u);
        vmin = std::min(vmin, vert.v);
        vmax = std::max(vmax, vert.v);
    }
    return std::format("draw prim={} vc={} tex={} ps={} xy=[{:.1f},{:.1f}..{:.1f},{:.1f}] "
                       "uv=[{:.4f},{:.4f}..{:.4f},{:.4f}] c0={:08X}\n",
                       prim_type, verts.size(), tex_slot, PsTag(hsl, add), xmin, ymin, xmax, ymax,
                       umin, vmin, umax, vmax, verts[0].color);
}

std::string TraceMaskRegion(unsigned int op, unsigned int layer, int x, int y, unsigned int w,
                            unsigned int h) {
    return std::format("mask op={} layer={} rect=[{},{} {}x{}]\n", op, layer, x, y, w, h);
}

std::string TraceLayerCommand(unsigned int cmd, unsigned int sub) {
    return std::format("layercmd cmd=0x{:X} sub={}\n", cmd, sub);
}

}
