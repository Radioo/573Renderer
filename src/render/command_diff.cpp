#include "render/command_diff.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "render/command_list.h"
#include "render/vertex_math.h"

namespace Render {

namespace {

bool NearF(float a, float b, float eps) {
    return std::fabs(a - b) <= eps;
}

std::string CmpSetLayer(const SetLayerCmd& a, const SetLayerCmd& b, float eps) {
    if (a.blend_mode != b.blend_mode) {
        return std::format("blend_mode {} != {}", a.blend_mode, b.blend_mode);
    }
    if (a.has_desc != b.has_desc) return std::format("has_desc {} != {}", a.has_desc, b.has_desc);
    if (!a.has_desc) return {};
    if (a.desc.id != b.desc.id) return std::format("hsv.id {} != {}", a.desc.id, b.desc.id);
    if (a.desc.valid != b.desc.valid) {
        return std::format("hsv.valid {} != {}", a.desc.valid, b.desc.valid);
    }
    if (!NearF(a.desc.hue, b.desc.hue, eps)) {
        return std::format("hsv.hue {} != {}", a.desc.hue, b.desc.hue);
    }
    if (!NearF(a.desc.sat, b.desc.sat, eps)) {
        return std::format("hsv.sat {} != {}", a.desc.sat, b.desc.sat);
    }
    if (!NearF(a.desc.val, b.desc.val, eps)) {
        return std::format("hsv.val {} != {}", a.desc.val, b.desc.val);
    }
    return {};
}

std::string CmpSetBlend(const SetBlendCmd& a, const SetBlendCmd& b) {
    if (a.mode != b.mode) return std::format("mode {} != {}", a.mode, b.mode);
    if (a.flags != b.flags) return std::format("flags {} != {}", a.flags, b.flags);
    return {};
}

std::string CmpVert(const AfpVertex& a, const AfpVertex& b, size_t i, float eps) {
    if (a.color != b.color)
        return std::format("vert {} color {:08X} != {:08X}", i, a.color, b.color);
    if (!NearF(a.x, b.x, eps)) return std::format("vert {} x {} != {}", i, a.x, b.x);
    if (!NearF(a.y, b.y, eps)) return std::format("vert {} y {} != {}", i, a.y, b.y);
    if (!NearF(a.z, b.z, eps)) return std::format("vert {} z {} != {}", i, a.z, b.z);
    if (!NearF(a.u, b.u, eps)) return std::format("vert {} u {} != {}", i, a.u, b.u);
    if (!NearF(a.v, b.v, eps)) return std::format("vert {} v {} != {}", i, a.v, b.v);
    return {};
}

std::string CmpDraw(const DrawCmd& a, const DrawCmd& b, float eps) {
    if (a.prim_type != b.prim_type) {
        return std::format("prim_type {} != {}", a.prim_type, b.prim_type);
    }
    if (a.tex_slot != b.tex_slot) return std::format("tex_slot {} != {}", a.tex_slot, b.tex_slot);
    if (a.hsl != b.hsl) return std::format("hsl {} != {}", a.hsl, b.hsl);
    if (a.add != b.add) return std::format("add {} != {}", a.add, b.add);
    if (a.matrix_ready != b.matrix_ready) {
        return std::format("matrix_ready {} != {}", a.matrix_ready, b.matrix_ready);
    }
    for (size_t i = 0; i < a.matrix.size(); i++) {
        if (!NearF(a.matrix.at(i), b.matrix.at(i), eps)) {
            return std::format("matrix {} {} != {}", i, a.matrix.at(i), b.matrix.at(i));
        }
    }
    if (a.verts.size() != b.verts.size()) {
        return std::format("vert count {} != {}", a.verts.size(), b.verts.size());
    }
    for (size_t i = 0; i < a.verts.size(); i++) {
        std::string r = CmpVert(a.verts[i], b.verts[i], i, eps);
        if (!r.empty()) return r;
    }
    return {};
}

std::string CmpMask(const MaskCmd& a, const MaskCmd& b) {
    if (a.op != b.op) return std::format("op {} != {}", a.op, b.op);
    if (a.layer != b.layer) return std::format("layer {} != {}", a.layer, b.layer);
    if (a.x != b.x) return std::format("x {} != {}", a.x, b.x);
    if (a.y != b.y) return std::format("y {} != {}", a.y, b.y);
    if (a.w != b.w) return std::format("w {} != {}", a.w, b.w);
    if (a.h != b.h) return std::format("h {} != {}", a.h, b.h);
    return {};
}

std::string CmpLayerCmd(const LayerCmdCmd& a, const LayerCmdCmd& b) {
    if (a.cmd != b.cmd) return std::format("cmd {:X} != {:X}", a.cmd, b.cmd);
    if (a.sub != b.sub) return std::format("sub {} != {}", a.sub, b.sub);
    return {};
}

std::string CmpSameType(const RenderCommand& a, const RenderCommand& b, float eps) {
    if (const auto* la = std::get_if<SetLayerCmd>(&a)) {
        return CmpSetLayer(*la, std::get<SetLayerCmd>(b), eps);
    }
    if (const auto* bl = std::get_if<SetBlendCmd>(&a)) {
        return CmpSetBlend(*bl, std::get<SetBlendCmd>(b));
    }
    if (const auto* dr = std::get_if<DrawCmd>(&a)) return CmpDraw(*dr, std::get<DrawCmd>(b), eps);
    if (const auto* mk = std::get_if<MaskCmd>(&a)) return CmpMask(*mk, std::get<MaskCmd>(b));
    return CmpLayerCmd(std::get<LayerCmdCmd>(a), std::get<LayerCmdCmd>(b));
}

}

std::optional<CommandDivergence> DiffCommandLists(const RenderCommandList& ref,
                                                  const RenderCommandList& actual, float epsilon) {
    const size_t n = std::min(ref.size(), actual.size());
    for (size_t i = 0; i < n; i++) {
        if (ref[i].index() != actual[i].index()) {
            return CommandDivergence{.index = i, .reason = "command type differs"};
        }
        std::string reason = CmpSameType(ref[i], actual[i], epsilon);
        if (!reason.empty()) return CommandDivergence{.index = i, .reason = std::move(reason)};
    }
    if (ref.size() != actual.size()) {
        return CommandDivergence{
            .index = n, .reason = std::format("length {} != {}", ref.size(), actual.size())};
    }
    return std::nullopt;
}

}
