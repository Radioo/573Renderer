#include "render/command_list.h"

#include <string>
#include <variant>

#include "render/command_trace.h"

namespace Render {

namespace {

struct FormatVisitor {
    std::string operator()(const SetLayerCmd& c) const {
        return TraceSetLayer(c.blend_mode, c.has_desc, c.desc);
    }
    std::string operator()(const SetBlendCmd& c) const { return TraceSetBlend(c.mode, c.flags); }
    std::string operator()(const DrawCmd& c) const {
        return TraceDraw(c.prim_type, c.tex_slot, c.hsl, c.add, c.verts);
    }
    std::string operator()(const MaskCmd& c) const {
        return TraceMaskRegion(c.op, c.layer, c.x, c.y, c.w, c.h);
    }
    std::string operator()(const LayerCmdCmd& c) const { return TraceLayerCommand(c.cmd, c.sub); }
};

}

std::string FormatCommand(const RenderCommand& cmd) {
    return std::visit(FormatVisitor{}, cmd);
}

std::string FormatCommandList(const RenderCommandList& list) {
    std::string out;
    for (const RenderCommand& c : list)
        out += FormatCommand(c);
    return out;
}

}
