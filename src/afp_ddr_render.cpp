#include <d3d9.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include "afp_ddr_render.h"
#include "afp_ddr_render_internal.h"
#include "support/engine_abi.h"
#include "support/env.h"
#include "support/log.h"
#include "render/blend_map.h"

namespace DdrRender {

IDirect3DDevice9* g_dev = nullptr;
int g_w = 1280;
int g_h = 720;
int g_draw_count = 0;
int g_frame = 0;
int g_shape_count = 0;
int g_max_count = 0;
int g_trunc_count = 0;
int g_loadmat_count = 0;
IDirect3DTexture9* g_tex[kMaxTex] = {};
int g_tex_count = 0;
D3DMATRIX g_proj;
D3DMATRIX g_world;
bool g_have_proj = false;
bool g_have_world = false;
bool g_in_mask_write = false;
RECT g_scissor = {0, 0, 0, 0};
int g_blend_mode = 0;
bool g_filter_on = false;
float g_filter_dh = 0.0F;
float g_filter_ds = 0.0F;
float g_filter_dl = 0.0F;
TexBindResolver g_tex_bind = nullptr;
BitmapQuery g_bitmap_query = nullptr;

int DumpFrame() {
    static int const f = Support::EnvInt("DDR_DUMP_FRAME").value_or(-1);
    return f;
}

void IdentityM(D3DMATRIX& m) {
    memset(&m, 0, sizeof(m));
    m._11 = m._22 = m._33 = m._44 = 1.0F;
}

namespace {

constexpr size_t kAfpAllocAlign = 16;

void ApplyTransforms() {
    if (g_dev == nullptr) return;
    D3DMATRIX ident;
    IdentityM(ident);
    g_dev->SetTransform(D3DTS_PROJECTION, g_have_proj ? &g_proj : &ident);
    D3DMATRIX view;
    IdentityM(view);
    g_dev->SetTransform(D3DTS_VIEW, &view);
    g_dev->SetTransform(D3DTS_WORLD, g_have_world ? &g_world : &ident);
}

}

void* AFP_CB Cb_Alloc(void* ctx, unsigned int size) {
    (void)ctx;
    return _aligned_malloc((size != 0U) ? size : 1, kAfpAllocAlign);
}
void* AFP_CB Cb_Realloc(void* ctx, void* p, unsigned int size) {
    (void)ctx;
    return _aligned_realloc(p, (size != 0U) ? size : 1, kAfpAllocAlign);
}
void AFP_CB Cb_Free(void* ctx, void* p) {
    (void)ctx;
    _aligned_free(p);
}

void AFP_CB Cb_InitFrame() {
    if (g_dev == nullptr) return;
    g_dev->SetVertexShader(nullptr);
    g_dev->SetPixelShader(nullptr);
    D3DVIEWPORT9 const vp = {0, 0, (DWORD)g_w, (DWORD)g_h, 0.0F, 1.0F};
    g_dev->SetViewport(&vp);
    g_dev->SetFVF(kFVF);
    g_dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_dev->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    g_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    g_dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    g_dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    g_dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    g_dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    g_dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    g_in_mask_write = false;
    g_filter_on = false;
    g_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    ApplyTransforms();
    g_shape_count = 0;
    g_max_count = 0;
    g_trunc_count = 0;
    g_loadmat_count = 0;
    if (g_frame < 2) LOG("DDR-R", "init_frame (frame %d)", g_frame);
}

void AFP_CB Cb_FinishFrame() {
    if (g_frame < 2) LOG("DDR-R", "finish_frame: %d draws", g_draw_count);
    int const df = DumpFrame();
    if (df >= 0 && g_frame >= df - 60 && g_frame <= df + 60) {
        LOG("DDR-R", "frame %d: %d draws, %d shapes, maxcnt=%d, ntrunc=%d, loadmat=%d", g_frame,
            g_draw_count, g_shape_count, g_max_count, g_trunc_count, g_loadmat_count);
    }
    if (g_frame == df && Support::EnvFlag("DDR_DUMP_TEX")) DumpAtlases();
    g_frame++;
}

void AFP_CB Cb_SetMask(int type, int level, int x, int y, int w, int h, int a7) {
    (void)level;
    (void)a7;
    if (g_dev == nullptr) return;
    static bool const no_mask = Support::EnvFlag("DDR_NO_MASK");
    if (no_mask) {
        g_in_mask_write = false;
        return;
    }
    if (g_frame == DumpFrame())
        LOG("DDR-R", "  set_mask type=%d level=%d rect=(%d,%d) %dx%d", type, level, x, y, w, h);
    g_in_mask_write = (type == 0);
    int l = (type == 1) ? x : 0;
    int t = (type == 1) ? y : 0;
    int rr = (type == 1) ? x + w : g_w;
    int bb = (type == 1) ? y + h : g_h;
    l = std::max(l, 0);
    t = std::max(t, 0);
    rr = std::min(rr, g_w);
    bb = std::min(bb, g_h);
    rr = std::max(rr, l);
    bb = std::max(bb, t);
    RECT const r{l, t, rr, bb};
    g_scissor = r;
    g_dev->SetScissorRect(&r);
    g_dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
}
void AFP_CB Cb_SetPriority(int p) {
    (void)p;
}

void AFP_CB Cb_SetBlend(int mode) {
    if (g_dev == nullptr) return;
    g_blend_mode = mode;
    static unsigned seen = 0;
    if (mode >= 0 && mode < 32 && ((seen & (1U << mode)) == 0U)) {
        seen |= (1U << mode);
        LOG("DDR-R", "Cb_SetBlend: NEW mode=%d (f%d)", mode, g_frame);
    }
    g_dev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
    g_dev->SetRenderState(D3DRS_SRCBLENDALPHA, Blend::kAlphaCoverage.src);
    g_dev->SetRenderState(D3DRS_DESTBLENDALPHA, Blend::kAlphaCoverage.dst);
    g_dev->SetRenderState(D3DRS_BLENDOPALPHA, Blend::kAlphaCoverage.op);
    const Blend::D3d9Blend bs = Blend::MapAfpMode((uint32_t)mode);
    g_dev->SetRenderState(D3DRS_BLENDOP, bs.op);
    g_dev->SetRenderState(D3DRS_SRCBLEND, bs.src);
    g_dev->SetRenderState(D3DRS_DESTBLEND, bs.dst);
}

void AFP_CB Cb_SetFilter(int a1, int a2, void* a3) {
    const auto* p = reinterpret_cast<const float*>(a3);
    if ((a1 == 100 || a1 == 101) && (a2 != 0) && (p != nullptr)) {
        g_filter_on = true;
        g_filter_dh = p[1] / 360.0F;
        g_filter_ds = p[2] / 100.0F;
        g_filter_dl = p[3] / 100.0F;
    } else {
        g_filter_on = false;
    }
    static int li = -99;
    static float ld = -99;
    static float ls = -99;
    static float ll = -99;
    if (g_filter_on && (a1 != li || g_filter_dh != ld || g_filter_ds != ls || g_filter_dl != ll)) {
        li = a1;
        ld = g_filter_dh;
        ls = g_filter_ds;
        ll = g_filter_dl;
        LOG("DDR-R", "Cb_SetFilter HSL id=%d dh=%.3f ds=%.3f dl=%.3f (f%d)", a1, g_filter_dh,
            g_filter_ds, g_filter_dl, g_frame);
    }
}
void AFP_CB Cb_SetDrawRect(const float* r) {
    (void)r;
}

void AFP_CB Cb_LoadMatrix(float* m2x3) {
    if (m2x3 == nullptr) {
        g_have_world = false;
        return;
    }
    IdentityM(g_world);
    g_world._11 = m2x3[0];
    g_world._12 = m2x3[1];
    g_world._21 = m2x3[2];
    g_world._22 = m2x3[3];
    g_world._41 = m2x3[4];
    g_world._42 = m2x3[5];
    g_have_world = true;
    g_loadmat_count++;
    if (g_frame == DumpFrame() && g_loadmat_count <= 40) {
        LOG("DDR-R", "  load_matrix2x3 #%d [%.3f %.3f %.3f %.3f tx=%.1f ty=%.1f]", g_loadmat_count,
            m2x3[0], m2x3[1], m2x3[2], m2x3[3], m2x3[4], m2x3[5]);
    }
    if (g_dev != nullptr) g_dev->SetTransform(D3DTS_WORLD, &g_world);
}

void AFP_CB Cb_LoadMatrix44(float* m) {
    if (m == nullptr) {
        g_have_world = false;
        return;
    }
    memcpy(&g_world, m, sizeof(D3DMATRIX));
    g_have_world = true;
    g_loadmat_count++;
    if (g_frame == DumpFrame() && g_loadmat_count <= 40) {
        LOG("DDR-R", "  load_matrix44 #%d [%.3f %.3f %.3f %.3f / %.3f %.3f %.3f %.3f]",
            g_loadmat_count, m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7]);
    }
    if (g_dev != nullptr) g_dev->SetTransform(D3DTS_WORLD, &g_world);
}

void AFP_CB Cb_LoadProj44(float* m) {
    if (m == nullptr) {
        g_have_proj = false;
        if (g_dev != nullptr) {
            D3DMATRIX ident;
            IdentityM(ident);
            g_dev->SetTransform(D3DTS_PROJECTION, &ident);
        }
        return;
    }
    memcpy(&g_proj, m, sizeof(D3DMATRIX));
    g_have_proj = true;
    if (g_frame < 1) {
        LOG("DDR-R",
            "load_proj44: [%.3f %.3f %.3f %.3f / %.3f %.3f %.3f %.3f / %.3f %.3f %.3f %.3f / %.3f "
            "%.3f %.3f %.3f]",
            m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], m[12], m[13],
            m[14], m[15]);
    }
    if (g_dev != nullptr) g_dev->SetTransform(D3DTS_PROJECTION, &g_proj);
}

void AFP_CB Cb_GetScreenSize(int* x, int* y, int* w, int* h) {
    if (x != nullptr) *x = 0;
    if (y != nullptr) *y = 0;
    if (w != nullptr) *w = g_w;
    if (h != nullptr) *h = g_h;
}
void AFP_CB Cb_GetNearFar(float* nr, float* fr) {
    static float n = 1.0F;
    static float f = 10000.0F;
    static bool const init = []() {
        std::optional<std::string> e = Support::EnvVar("DDR_NEARFAR");
        if (e) {
            float a = 0;
            float b = 0;
            if (sscanf_s(e->c_str(), "%f,%f", &a, &b) == 2) {
                n = a;
                f = b;
            }
        }
        return true;
    }();
    (void)init;
    static int calls = 0;
    if (++calls <= 3) LOG("DDR-R", "Cb_GetNearFar call #%d -> near=%.3f far=%.1f", calls, n, f);
    if (nr != nullptr) *nr = n;
    if (fr != nullptr) *fr = f;
}

int __stdcall Cb_GetBitmapInfo(unsigned* out_id, int* out_w, int* out_h, float* out_u0,
                               float* out_u1, float* out_v0, float* out_v1, const char* name) {
    static int s_queries = 0;
    if (g_bitmap_query == nullptr || name == nullptr) {
        if (s_queries++ < 8) LOG("DDR-R", "get_bitmap_info('%s'): no provider", name ? name : "");
        return 0;
    }
    unsigned id = 0;
    int w = 0;
    int h = 0;
    float u0 = 0.0F;
    float u1 = 1.0F;
    float v0 = 0.0F;
    float v1 = 1.0F;
    const bool hit = g_bitmap_query(name, &id, &w, &h, &u0, &u1, &v0, &v1);
    if (s_queries++ < 12) {
        LOG("DDR-R", "get_bitmap_info('%s') -> %s id=%u %dx%d uv=[%.3f,%.3f]x[%.3f,%.3f]", name,
            hit ? "hit" : "MISS", id, w, h, u0, u1, v0, v1);
    }
    if (!hit) return 0;
    if (out_id != nullptr) *out_id = id;
    if (out_w != nullptr) *out_w = w;
    if (out_h != nullptr) *out_h = h;
    if (out_u0 != nullptr) *out_u0 = u0;
    if (out_u1 != nullptr) *out_u1 = u1;
    if (out_v0 != nullptr) *out_v0 = v0;
    if (out_v1 != nullptr) *out_v1 = v1;
    return 1;
}

}
