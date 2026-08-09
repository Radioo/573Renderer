#include <d3d9.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include "afp_ddr_render_internal.h"
#include "support/engine_abi.h"
#include "support/env.h"
#include "support/log.h"
#include "formats/hsl_adjust.h"
#include "render/prim_layout.h"

namespace DdrRender {
namespace {

void LogPixColDiag(int count, const int* params) {
    const auto* dc = reinterpret_cast<const float*>(&params[4]);
    const auto* pc = reinterpret_cast<const float*>(&params[8]);
    float const pmn = std::min({pc[0], pc[1], pc[2]});
    float const pmx = std::max({pc[0], pc[1], pc[2]});
    bool const pixtint = (pmx - pmn > 0.10F) || (pmx < 0.90F && pmx > 0.02F);
    static int s_diag = 0;
    if (pixtint && s_diag++ < 80) {
        LOG("DDR-R",
            "PIXCOL f%d draw#%d tex=%#x cnt=%d mod=%.2f,%.2f,%.2f,%.2f pix=%.2f,%.2f,%.2f,%.2f",
            g_frame, g_draw_count, params[2], count, dc[0], dc[1], dc[2], dc[3], pc[0], pc[1],
            pc[2], pc[3]);
    }
}

bool DrawGateAllows() {
    static int const s_only_draw = Support::EnvInt("DDR_ONLY_DRAW").value_or(-1);
    if (s_only_draw >= 0 && g_draw_count != s_only_draw) return false;
    static int const s_draw_min = Support::EnvInt("DDR_DRAW_MIN").value_or(-1);
    static int const s_draw_max = Support::EnvInt("DDR_DRAW_MAX").value_or(-1);
    if (s_draw_min >= 0 && g_draw_count < s_draw_min) return false;
    if (s_draw_max >= 0 && g_draw_count > s_draw_max) return false;
    return true;
}

struct DrawBBox {
    float bbx0 = 1e9F;
    float bby0 = 1e9F;
    float bbx1 = -1e9F;
    float bby1 = -1e9F;
    float uu0 = 1e9F;
    float vv0 = 1e9F;
    float uu1 = -1e9F;
    float vv1 = -1e9F;
    uint32_t first_vcol = 0xFFFFFFFFU;
};

int BuildVertices(const float* vtx, int count, const Render::VtxLayout& lay, D3DCOLOR modulate,
                  bool dump, Vtx* buf, DrawBBox& bb) {
    int const n = count < kMaxDrawVerts ? count : kMaxDrawVerts;
    g_max_count = std::max(count, g_max_count);
    if (count > kMaxDrawVerts) g_trunc_count++;
    for (int i = 0; i < n; i++) {
        const float* s = vtx + ((size_t)i * lay.stride);
        int o = 0;
        float u = 0;
        float v = 0;
        if (lay.has_uv) {
            u = s[o];
            v = s[o + 1];
            o += 2;
        }
        if (lay.skip2) o += 2;
        uint32_t vcol = 0xFFFFFFFFU;
        if (lay.has_vcol) {
            vcol = (uint32_t)(int64_t)s[o];
            o += 1;
        }
        float x = 0;
        float y = 0;
        if (lay.pos3 || lay.pos2) {
            x = s[o];
            y = s[o + 1];
        }
        if (i == 0) bb.first_vcol = vcol;
        Vtx& d = buf[i];
        d.x = x - 0.5F;
        d.y = y - 0.5F;
        d.z = 0.0F;
        d.rhw = 1.0F;
        uint32_t c = lay.has_vcol ? Render::MulARGB(vcol, (uint32_t)modulate) : (uint32_t)modulate;
        if (g_filter_on) c = Hsl::AdjustArgb(c, g_filter_dh, g_filter_ds, g_filter_dl);
        d.color = c;
        d.u = u;
        d.v = v;
        if (dump) {
            bb.uu0 = std::min(u, bb.uu0);
            bb.uu1 = std::max(u, bb.uu1);
            bb.vv0 = std::min(v, bb.vv0);
            bb.vv1 = std::max(v, bb.vv1);
            bb.bbx0 = std::min(x, bb.bbx0);
            bb.bbx1 = std::max(x, bb.bbx1);
            bb.bby0 = std::min(y, bb.bby0);
            bb.bby1 = std::max(y, bb.bby1);
        }
    }
    return n;
}

D3DCOLOR ApplyLineAlpha(D3DCOLOR modulate, const float* vtx, const Render::VtxLayout& lay,
                        int afp_tex) {
    static std::optional<std::string> s_line_alpha = Support::EnvVar("DDR_LINE_ALPHA");
    if (!s_line_alpha || !lay.has_uv || afp_tex <= 0) return modulate;
    float const fv = vtx[1];
    if (fv < 0.105F || fv > 0.120F) return modulate;
    auto fa = (float)atof(s_line_alpha->c_str());
    int a = (int)((float)((modulate >> 24) & 0xFF) * fa);
    a = std::max(a, 0);
    a = std::min(a, 255);
    return (modulate & 0x00FFFFFFU) | ((unsigned)a << 24);
}

void LogDrawDump(int count, const int* params, const Render::VtxLayout& lay, const DrawBBox& bb,
                 const Vtx* buf, int n) {
    int const afp_type = params[0];
    int const flags = params[1];
    int const afp_tex = params[2];
    const auto* col = reinterpret_cast<const float*>(&params[4]);
    const auto* pc = reinterpret_cast<const float*>(&params[8]);
    LOG("DDR-R", "  draw #%d pix=%.3f,%.3f,%.3f,%.3f p3=%08X", g_draw_count, pc[0], pc[1], pc[2],
        pc[3], params[3]);
    LOG("DDR-R",
        "  draw #%d type=%d flags=0x%x blend=%d vcol=%d v0col=%08X stride=%d pos3=%d tex=%#x "
        "bind=%d cnt=%d bbox=(%.0f,%.0f)-(%.0f,%.0f) uv=(%.3f,%.3f)-(%.3f,%.3f) "
        "col=%.2f,%.2f,%.2f,%.2f",
        g_draw_count, afp_type, flags, g_blend_mode, lay.has_vcol ? 1 : 0, bb.first_vcol,
        lay.stride, lay.pos3 ? 1 : 0, afp_tex,
        (g_tex_bind && afp_tex > 0) ? g_tex_bind((unsigned)afp_tex) : -1, count, bb.bbx0, bb.bby0,
        bb.bbx1, bb.bby1, bb.uu0, bb.vv0, bb.uu1, bb.vv1, col[0], col[1], col[2], col[3]);
    static int const s_dump_draw = Support::EnvInt("DDR_DUMP_DRAW").value_or(-1);
    if (s_dump_draw >= 0 && g_draw_count == s_dump_draw) {
        for (int i = 0; i < n && i < 48; i++) {
            LOG("DDR-R", "    v%d (%.1f,%.1f) uv(%.3f,%.3f)", i, buf[i].x, buf[i].y, buf[i].u,
                buf[i].v);
        }
    }
}

void LogTrackBars(int count, const Vtx* buf) {
    static int const s_track_bars = Support::EnvInt("DDR_TRACK_BARS").value_or(0);
    if ((s_track_bars == 0) || count < 90 || count > 99) return;
    float const ax = buf[0].x;
    float const ay = buf[0].y;
    float const bx = buf[2].x;
    float const by = buf[2].y;
    float const cx = buf[1].x;
    float const cy = buf[1].y;
    float const wdt = (((bx - ax) * (bx - ax)) + ((by - ay) * (by - ay)));
    float const len = (((cx - ax) * (cx - ax)) + ((cy - ay) * (cy - ay)));
    auto ang = (float)(atan2(by - ay, bx - ax) * 57.29578);
    LOG("DDR-R",
        "BAR f%d d#%d width=%.1f len=%.1f wedge_ang=%.1f corners a(%.0f,%.0f) b(%.0f,%.0f) "
        "c(%.0f,%.0f)",
        g_frame, g_draw_count, sqrtf(wdt), sqrtf(len), ang, ax, ay, bx, by, cx, cy);
}

}

void AFP_CB Cb_DrawPrimitive(const float* vtx, int count, int* params, void* a4) {
    (void)a4;
    g_draw_count++;
    if ((g_dev == nullptr) || (vtx == nullptr) || (params == nullptr) || count <= 0) return;
    LogPixColDiag(count, params);
    if (g_in_mask_write) {
        if (g_frame == DumpFrame()) {
            const auto* dc = reinterpret_cast<const float*>(&params[4]);
            LOG("DDR-R",
                "  SKIP-maskwrite draw#%d type=%d flags=0x%x tex=%#x cnt=%d "
                "col=%.2f,%.2f,%.2f,%.2f",
                g_draw_count, params[0], params[1], params[2], count, dc[0], dc[1], dc[2], dc[3]);
        }
        return;
    }
    if (!DrawGateAllows()) return;

    int const afp_type = params[0];
    int const flags = params[1];
    int const afp_tex = params[2];
    const auto* col = reinterpret_cast<const float*>(&params[4]);
    auto cl = [](float f) {
        int const v = (int)std::lroundf(f * 255.0F);
        return std::clamp(v, 0, 255);
    };
    D3DCOLOR modulate = D3DCOLOR_ARGB(cl(col[3]), cl(col[0]), cl(col[1]), cl(col[2]));

    Render::VtxLayout const lay = Render::DecodeVtxLayout(flags);
    if (lay.stride == 0) return;

    modulate = ApplyLineAlpha(modulate, vtx, lay, afp_tex);

    if (g_frame < 1 && g_draw_count <= 8) {
        LOG("DDR-R", "draw #%d type=%d flags=0x%x tex=%#x cnt=%d col=%.2f,%.2f,%.2f,%.2f stride=%d",
            g_draw_count, afp_type, flags, afp_tex, count, col[0], col[1], col[2], col[3],
            lay.stride);
    }

    IDirect3DTexture9* tex = nullptr;
    if (afp_tex > 0) {
        int const bind = (g_tex_bind != nullptr) ? g_tex_bind((unsigned)afp_tex) : 0;
        if (bind >= 0 && bind < g_tex_count) tex = g_tex[bind];
    }
    g_dev->SetTexture(0, tex);

    static Vtx buf[kMaxDrawVerts];
    bool const dump = (g_frame == DumpFrame());
    DrawBBox bb;
    int const n = BuildVertices(vtx, count, lay, modulate, dump, buf, bb);

    if (dump) LogDrawDump(count, params, lay, bb, buf, n);

    int prims = 0;
    D3DPRIMITIVETYPE const pt = Render::MapPrimType(afp_type, n, prims);

    LogTrackBars(count, buf);

    if (prims > 0) g_dev->DrawPrimitiveUP(pt, prims, buf, sizeof(Vtx));
}

void AFP_CB Cb_DrawPrimitiveLegacy(const float* vtx, int count, int prim_type, unsigned attr,
                                   int a5, int a6, const float* c0, const float* c1, void* ctx) {
    if (g_frame < 1 && g_draw_count <= 8) {
        LOG("DDR-R", "legacy draw_primitive type=%d attr=%#x a5=%#x a6=%#x", prim_type, attr,
            (unsigned)a5, (unsigned)a6);
    }
    int params[12] = {};
    params[0] = prim_type;
    params[1] = (int)attr;
    params[2] = a5;
    params[3] = a6;
    auto copy4 = [](float* dst, const float* src) {
        if (src == nullptr) return;
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
    };
    copy4(reinterpret_cast<float*>(&params[4]), c0);
    copy4(reinterpret_cast<float*>(&params[8]), c1);
    Cb_DrawPrimitive(vtx, count, params, ctx);
}

namespace {

void LogShapeDraw(int texture_slot, uint32_t modulate, const Vtx* buf, int n) {
    float x0 = 1e9F;
    float y0 = 1e9F;
    float x1 = -1e9F;
    float y1 = -1e9F;
    float u0 = 1e9F;
    float v0 = 1e9F;
    float u1 = -1e9F;
    float v1 = -1e9F;
    for (int i = 0; i < n; i++) {
        x0 = std::min(x0, buf[i].x);
        x1 = std::max(x1, buf[i].x);
        y0 = std::min(y0, buf[i].y);
        y1 = std::max(y1, buf[i].y);
        u0 = std::min(u0, buf[i].u);
        u1 = std::max(u1, buf[i].u);
        v0 = std::min(v0, buf[i].v);
        v1 = std::max(v1, buf[i].v);
    }
    LOG("DDR-R",
        "  shape draw#%d tex=%d tris=%d mod=%08X bbox=(%.0f,%.0f)-(%.0f,%.0f) "
        "uv=(%.3f,%.3f)-(%.3f,%.3f) blend=%d scissor=(%ld,%ld)-(%ld,%ld)",
        g_draw_count, texture_slot, n / 3, modulate, x0, y0, x1, y1, u0, v0, u1, v1, g_blend_mode,
        g_scissor.left, g_scissor.top, g_scissor.right, g_scissor.bottom);
}

}

void DrawShapeTriangles(int texture_slot, uint32_t modulate, const float* positions,
                        const float* uvs, const unsigned char* colors, const uint16_t* indices,
                        int index_count, int vertex_count) {
    if ((g_dev == nullptr) || positions == nullptr || indices == nullptr) return;
    if (index_count < 3 || vertex_count <= 0) return;
    if (g_in_mask_write || !DrawGateAllows()) return;

    IDirect3DTexture9* tex = TextureAt(texture_slot);
    g_dev->SetTexture(0, tex);
    float du = 0.0F;
    float dv = 0.0F;
    UvBias(tex, du, dv);

    static Vtx buf[kMaxDrawVerts];
    int const n = std::min(index_count - (index_count % 3), kMaxDrawVerts);
    for (int i = 0; i < n; i++) {
        int const vi = indices[i];
        Vtx& d = buf[i];
        if (vi >= vertex_count) return;
        float const x = positions[(size_t)vi * 2];
        float const y = positions[((size_t)vi * 2) + 1];
        d.x = ((g_world._11 * x) + (g_world._21 * y) + g_world._41) - 0.5F;
        d.y = ((g_world._12 * x) + (g_world._22 * y) + g_world._42) - 0.5F;
        d.z = 0.0F;
        d.rhw = 1.0F;
        uint32_t vcol = 0xFFFFFFFFU;
        if (colors != nullptr) vcol = (uint32_t)colors[vi] << 16U;
        uint32_t c = Render::MulARGB(vcol, modulate);
        if (g_filter_on) c = Hsl::AdjustArgb(c, g_filter_dh, g_filter_ds, g_filter_dl);
        d.color = c;
        d.u = (uvs != nullptr) ? uvs[(size_t)vi * 2] + du : 0.0F;
        d.v = (uvs != nullptr) ? uvs[((size_t)vi * 2) + 1] + dv : 0.0F;
    }
    g_draw_count++;
    g_shape_count++;
    if (g_frame == DumpFrame()) LogShapeDraw(texture_slot, modulate, buf, n);
    g_dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, n / 3, buf, sizeof(Vtx));
}

}
