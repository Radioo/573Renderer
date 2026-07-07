#include <cstdint>
#include "gpu_context.h"
#include "render/vertex_math.h"
#include "render_backend.h"
#include "afp_d3d9_internal.h"
#include "render/blend_map.h"
#include "render/command_list.h"
#include "render_executor.h"
#include "render/hsv_filter.h"
#include "support/log.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>
#include <intrin.h>
#include <windows.h>

namespace AfpD3D9 {

void AfpScreamUnimpl(const char* what, const void* caller) {
    auto pc = reinterpret_cast<uintptr_t>(caller);
    const char* mod = "?";
    uintptr_t off = pc;
    HMODULE afp = GetModuleHandleA("afp-core.dll");
    HMODULE afpu = GetModuleHandleA("afp-utils.dll");
    const auto ab = reinterpret_cast<uintptr_t>(afp);
    const auto ub = reinterpret_cast<uintptr_t>(afpu);
    if (ab != 0U && pc >= ab && pc < ab + 0x800000) {
        mod = "afp-core";
        off = pc - ab;
    } else if (ub != 0U && pc >= ub && pc < ub + 0x800000) {
        mod = "afp-utils";
        off = pc - ub;
    }
    LOG("AfpD3D9", "############################################################");
    LOG("AfpD3D9", "## UNIMPLEMENTED afp callback HIT: %s", what);
    LOG("AfpD3D9", "## afp relies on this - REVERSE the game and implement it ASAP.");
    LOG("AfpD3D9", "## called from %s+0x%llx", mod, (unsigned long long)off);
    LOG("AfpD3D9", "############################################################");
}

namespace {
int g_last_setlayer_blend = 0;
}

bool InMaskWrite() {
    return g_gpu.in_mask_write;
}
void ResetMaskWrite() {
    g_gpu.in_mask_write = false;
}

void __cdecl SetLayer(unsigned int blend_mode, int zero, const unsigned char* hsv_desc) {
    (void)zero;
    g_last_setlayer_blend = (int)blend_mode;
    if (reinterpret_cast<uintptr_t>(hsv_desc) >= 0x800000000000ULL) hsv_desc = nullptr;
    g_gpu.hsv_desc_ptr = hsv_desc;
    if (hsv_desc != nullptr) memcpy(g_gpu.hsv_captured, hsv_desc, 16);
    static int hsv_log = 0;
    if ((hsv_desc != nullptr) && hsv_log < 8) {
        unsigned short const valid = *(const unsigned short*)(hsv_desc + 2);
        float const hue = *(const float*)(hsv_desc + 4);
        float const sat = *(const float*)(hsv_desc + 8);
        float const val = *(const float*)(hsv_desc + 12);
        LOG("HSV", "SetLayer/+0x28 #%d arg0=0x%x id=%u valid=%u mono=%u hue=%.3f sat=%.3f val=%.3f",
            hsv_log, blend_mode, hsv_desc[0], valid, hsv_desc[3], hue, sat, val);
        hsv_log++;
    }
    if ((g_gpu.device == nullptr) && !g_gpu.deferred_replay) return;

    Render::SetLayerCmd cmd{.blend_mode = blend_mode, .has_desc = false, .desc = {}};
    if (!g_gpu.deferred_replay) RenderExec::Execute(g_gpu.device, cmd);
    if ((g_gpu.cmd_list != nullptr) || g_gpu.deferred_replay) {
        if (hsv_desc != nullptr) {
            const Render::HsvDescriptor parsed = Render::ParseHsvDescriptor({hsv_desc, 16});
            if (Render::HsvFilterActive(parsed)) {
                cmd.has_desc = true;
                cmd.desc = parsed;
            }
        }
        if (g_gpu.cmd_list != nullptr) g_gpu.cmd_list->emplace_back(cmd);
        if (g_gpu.deferred_replay) g_gpu.frame_cmds.emplace_back(cmd);
    }
}

void __cdecl DrawPrimitive([[maybe_unused]] unsigned int prim) {}

namespace {
uint8_t g_active_blend_mode = 0;
}

void __cdecl SetBlend(unsigned int blend_mode, unsigned char flags, void* extra) {
    if ((g_gpu.device == nullptr) && !g_gpu.deferred_replay) return;
    const Render::SetBlendCmd cmd{.mode = blend_mode, .flags = flags};
    if (g_gpu.cmd_list != nullptr) g_gpu.cmd_list->emplace_back(cmd);
    if (g_gpu.deferred_replay) g_gpu.frame_cmds.emplace_back(cmd);

    static int blend_log = 0;
    if (blend_log < 20) {
        LOG("AfpD3D9", "SetBlend(mode=%u, flags=%u, extra=%p)", blend_mode, flags, extra);
        blend_log++;
    }

    g_active_blend_mode = (uint8_t)(blend_mode & 0xFF);
    if (!g_gpu.deferred_replay) RenderExec::Execute(g_gpu.device, cmd);
}

void __cdecl SetMask([[maybe_unused]] unsigned int type, [[maybe_unused]] unsigned char level,
                     [[maybe_unused]] void* data) {}

namespace {

int ResolveBoundTexture(uint32_t tex_ref, int& bound_tex_w, int& bound_tex_h) {
    bound_tex_w = 0;
    bound_tex_h = 0;
    if (tex_ref == 0) return -1;
    if ((tex_ref & 0x78000000U) == 0x08000000U && (g_gpu.afpu_get_tex_slot != nullptr)) {
        uint32_t const slot_id = g_gpu.afpu_get_tex_slot(tex_ref);
        static int resolve_log = 0;
        if (resolve_log < 10) {
            unsigned const pkg_idx = (tex_ref >> 15) & 0xFFU;
            unsigned const slot_in_pkg = tex_ref & 0x7FFFU;
            LOG("AfpD3D9", "SubmitGeo tex_ref=0x%08x (pkg=%u,slot=%u) -> our slot %u %s", tex_ref,
                pkg_idx, slot_in_pkg, slot_id,
                (slot_id > 0 && slot_id < kTexSlotCount && g_gpu.textures[slot_id]) ? "HIT"
                                                                                    : "MISS");
            resolve_log++;
        }
        if (slot_id > 0 && slot_id < kTexSlotCount && (g_gpu.textures[slot_id] != nullptr)) {
            bound_tex_w = g_gpu.texture_dims_w[slot_id];
            bound_tex_h = g_gpu.texture_dims_h[slot_id];
            return (int)slot_id;
        }
        return -1;
    }
    int const tex_idx = static_cast<int>(tex_ref & 0xFFFF) + 1;
    if (tex_idx > 0 && tex_idx < kTexSlotCount && (g_gpu.textures[tex_idx] != nullptr)) {
        bound_tex_w = g_gpu.texture_dims_w[tex_idx];
        bound_tex_h = g_gpu.texture_dims_h[tex_idx];
        return tex_idx;
    }
    return -1;
}

void LogSeamVertProbe(const AfpVertex* verts, int vert_count, uint32_t tex_ref) {
    static int vlog = 0;
    if (vlog >= 40) return;
    float min_x = verts[0].x;
    float max_x = verts[0].x;
    for (int i = 1; i < vert_count; i++) {
        min_x = std::min(verts[i].x, min_x);
        max_x = std::max(verts[i].x, max_x);
    }
    if (min_x <= 540.0F && max_x >= 539.0F) {
        LOG("VertProbe", "Draw #%d span_x=[%.4f..%.4f] vc=%d tex=0x%08x", g_gpu.draw_count, min_x,
            max_x, vert_count, tex_ref);
        for (int i = 0; i < vert_count && i < 32; i++) {
            LOG("VertProbe", "  v[%d]=(%.4f, %.4f) uv=(%.5f, %.5f)", i, verts[i].x, verts[i].y,
                verts[i].u, verts[i].v);
        }
        vlog++;
    }
}

void LogQproDrawProbe(const AfpVertex* verts, int vert_count, uint32_t tex_ref) {
    static int qpro_probe_log = 0;
    if (!g_gpu.qpro_draw_probe || qpro_probe_log >= 40) return;
    qpro_probe_log++;
    float umin = verts[0].u;
    float umax = verts[0].u;
    float vmin = verts[0].v;
    float vmax = verts[0].v;
    float xmin = verts[0].x;
    float xmax = verts[0].x;
    float ymin = verts[0].y;
    float ymax = verts[0].y;
    for (int i = 1; i < vert_count; i++) {
        umin = std::min(verts[i].u, umin);
        umax = std::max(verts[i].u, umax);
        vmin = std::min(verts[i].v, vmin);
        vmax = std::max(verts[i].v, vmax);
        xmin = std::min(verts[i].x, xmin);
        xmax = std::max(verts[i].x, xmax);
        ymin = std::min(verts[i].y, ymin);
        ymax = std::max(verts[i].y, ymax);
    }
    int hid = -1;
    int hvalid = -1;
    float hhue = 0.0F;
    float hsat = 0.0F;
    float hval = 0.0F;
    if (g_gpu.hsv_desc_ptr != nullptr) {
        hid = g_gpu.hsv_desc_ptr[0];
        hvalid = *(const unsigned short*)(g_gpu.hsv_desc_ptr + 2);
        hhue = *(const float*)(g_gpu.hsv_desc_ptr + 4);
        hsat = *(const float*)(g_gpu.hsv_desc_ptr + 8);
        hval = *(const float*)(g_gpu.hsv_desc_ptr + 12);
    }
    LOG("QproDraw",
        "draw#%d vc=%d tex=0x%08x blend=%d x=[%.1f..%.1f w=%.1f] y=[%.1f..%.1f h=%.1f] "
        "u=[%.4f..%.4f] v=[%.4f..%.4f] hsv{id=%d valid=%d h=%.2f s=%.2f v=%.2f}",
        g_gpu.draw_count, vert_count, tex_ref, g_last_setlayer_blend, xmin, xmax, xmax - xmin, ymin,
        ymax, ymax - ymin, umin, umax, vmin, vmax, hid, hvalid, hhue, hsat, hval);
}

void ResolveFilterState(int prim_count, bool& hsl_active, bool& add_active,
                        std::array<float, 4>& hsv_c1, std::array<float, 4>& hsv_c3) {
    hsl_active = false;
    add_active = false;
    if (prim_count <= 0) return;
    if ((g_gpu.hsv_desc_ptr != nullptr) && (g_gpu.afp_hsl_ps != nullptr)) {
        const Render::HsvDescriptor cur = Render::ParseHsvDescriptor({g_gpu.hsv_desc_ptr, 16});
        if (Render::HsvFilterActive(cur)) {
            const Render::HsvDescriptor cap = Render::ParseHsvDescriptor({g_gpu.hsv_captured, 16});
            hsv_c1 = Render::HsvShaderConstant(cap);
            hsv_c3 = Render::HsvShaderConstant(cur);
            hsl_active = true;
        }
    }
    g_gpu.hsv_desc_ptr = nullptr;
    if (!hsl_active && (g_gpu.afp_add_ps != nullptr) &&
        Blend::IsAdditiveAfpMode((uint32_t)g_last_setlayer_blend))
        add_active = true;
}

}

namespace {

bool SubmitPreflightOk(int a2, const void* geo_data) {
    if (((g_gpu.device == nullptr) && !g_gpu.deferred_replay) || (geo_data == nullptr)) {
        g_gpu.submit_rejected_device++;
        if (g_gpu.submit_rejected_device <= 5) {
            LOG("AfpD3D9", "SubmitGeometry REJECTED: device=%p geo_data=%p", g_gpu.device,
                geo_data);
        }
        return false;
    }
    if (g_gpu.in_mask_write) {
        static int mask_skip_log = 0;
        if (mask_skip_log < 10) {
            LOG("AfpD3D9", "SubmitGeometry SKIP (mask-write) vc=%d", a2);
            mask_skip_log++;
        }
        return false;
    }
    return true;
}

void LogSubmitGeoProbe(const void* a1, int a2, const DWORD* geo, const void* tex_ptr) {
    static int geo_log = 0;
    if (geo_log >= 10) return;
    const auto* a1d = (const DWORD*)a1;
    LOG("AfpD3D9", "SubmitGeo a1=[0x%x 0x%x 0x%x 0x%x] a2=%d geo=[0x%x 0x%x 0x%x 0x%x] tex=%p",
        a1d[0], a1d[1], a1d[2], a1d[3], a2, geo[0], geo[1], geo[2], geo[3], tex_ptr);
    LOG("AfpD3D9", "  geo floats: [4]=%f [5]=%f [6]=%f [7]=%f", *(const float*)(geo + 4),
        *(const float*)(geo + 5), *(const float*)(geo + 6), *(const float*)(geo + 7));
    geo_log++;
}

bool RejectBadVertCount(int vert_count, int prim_type, int flags, uint32_t tex) {
    if (vert_count > 0 && vert_count <= 10000) return false;
    g_gpu.submit_rejected_vc++;
    if (g_gpu.submit_rejected_vc <= 10) {
        LOG("AfpD3D9", "SubmitGeometry REJECTED vc=%d (prim=%d flags=0x%x tex=0x%x)", vert_count,
            prim_type, flags, tex);
    }
    return true;
}

void RecordAndDispatchDraw(Render::DrawCmd& cmd, const AfpVertex* verts, int vert_count,
                           int prim_count) {
    if (cmd.matrix_ready)
        std::copy_n(static_cast<const float*>(g_gpu.current_matrix), 16, cmd.matrix.begin());
    if (((g_gpu.cmd_list != nullptr) && prim_count > 0) || g_gpu.deferred_replay)
        cmd.verts.assign(verts, verts + vert_count);
    if ((g_gpu.cmd_list != nullptr) && prim_count > 0) g_gpu.cmd_list->emplace_back(cmd);
    if (g_gpu.deferred_replay) {
        g_gpu.frame_cmds.emplace_back(std::move(cmd));
        return;
    }
    RenderExec::Execute(g_gpu.device, g_gpu, cmd, {verts, static_cast<size_t>(vert_count)});
}

}

void __cdecl SubmitGeometry(void* a1, int a2, void* geo_data, void* tex_ptr) {
    if (!SubmitPreflightOk(a2, geo_data)) return;

    auto* geo = (DWORD*)geo_data;
    int vert_count = a2;
    int const prim_type = (int)geo[0];

    LogSubmitGeoProbe(a1, a2, geo, tex_ptr);

    int const flags = (int)geo[1];
    if (RejectBadVertCount(vert_count, prim_type, flags, geo[2])) return;
    auto* src = (float*)a1;
    auto* uv_data = (float*)(geo + 4);

    uint32_t const tex_ref = geo[2];
    int bound_tex_w = 0;
    int bound_tex_h = 0;
    int const bound_slot = ResolveBoundTexture(tex_ref, bound_tex_w, bound_tex_h);

    const std::array<float, 4> shape_color = {*(float*)(geo + 4), *(float*)(geo + 5),
                                              *(float*)(geo + 6), *(float*)(geo + 7)};

    AfpVertex verts[256];
    vert_count = std::min(vert_count, 256);
    Render::DecodeAfpVertices({src, static_cast<size_t>(vert_count) * 8},
                              {uv_data, static_cast<size_t>(vert_count) * 2}, flags,
                              {verts, static_cast<size_t>(vert_count)});
    Render::InsetQuadUvs({verts, static_cast<size_t>(vert_count)}, prim_type, bound_tex_w,
                         bound_tex_h);

    if (g_gpu.draw_count < 20) {
        LOG("AfpD3D9", "Draw #%d: %dv prim=%d flags=0x%x tex=0x%08x v0=(%.4f,%.4f)",
            g_gpu.draw_count, vert_count, prim_type, flags, tex_ref, verts[0].x, verts[0].y);
    }

    LogSeamVertProbe(verts, vert_count, tex_ref);

    const int prim_count = Render::AfpPrimCount(prim_type, vert_count);

    LogQproDrawProbe(verts, vert_count, tex_ref);

    bool hsl_active = false;
    bool add_active = false;
    std::array<float, 4> hsv_c1{};
    std::array<float, 4> hsv_c3{};
    ResolveFilterState(prim_count, hsl_active, add_active, hsv_c1, hsv_c3);

    Render::DrawCmd cmd{.prim_type = prim_type,
                        .tex_slot = bound_slot,
                        .hsl = hsl_active,
                        .add = add_active,
                        .matrix_ready = g_gpu.current_matrix_ready,
                        .verts = {},
                        .shape_color = shape_color,
                        .hsv_c1 = hsv_c1,
                        .hsv_c3 = hsv_c3,
                        .matrix = {}};
    RecordAndDispatchDraw(cmd, verts, vert_count, prim_count);
}

}
