#include <d3d9.h>
#include <cstdint>
#include <optional>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include "afp_ddr_render.h"
#include "afp_ddr_render_shape.h"
#include "support/engine_abi.h"
#include "support/env.h"
#include "support/log.h"
#include "formats/dxt_decode.h"
#include "formats/hsl_adjust.h"
#include "render/blend_map.h"
#include "render/prim_layout.h"
#include <cstring>
#include <cstdlib>

namespace DdrRender {
namespace {

IDirect3DDevice9* g_dev = nullptr;
int g_w = 1280, g_h = 720;
int g_draw_count = 0;
int g_frame = 0;

int g_shape_count = 0;
int g_max_count = 0;
int g_trunc_count = 0;
int g_loadmat_count = 0;
int DumpFrame() {
    static int const f = Support::EnvInt("DDR_DUMP_FRAME").value_or(-1);
    return f;
}

struct Vtx {
    float x, y, z, rhw;
    D3DCOLOR color;
    float u, v;
};
constexpr DWORD kFVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

constexpr int kMaxTex = 4096;
IDirect3DTexture9* g_tex[kMaxTex] = {};
int g_tex_count = 0;

D3DMATRIX g_proj, g_world;
bool g_have_proj = false, g_have_world = false;

void IdentityM(D3DMATRIX& m) {
    memset(&m, 0, sizeof(m));
    m._11 = m._22 = m._33 = m._44 = 1.0F;
}

bool g_in_mask_write = false;
RECT g_scissor = {0, 0, 0, 0};

void DecodeDxtToRect(const uint8_t* src, int w, int h, uint8_t* dst, int pitch, bool dxt5) {
    const uint32_t fmt = dxt5 ? Dxt::kFmtDxt5 : Dxt::kFmtDxt1;
    const size_t src_size = Dxt::EncodedSize(fmt, w, h);
    const size_t dst_size = ((size_t)(h - 1) * (size_t)pitch) + ((size_t)w * 4);
    Dxt::Decompress(fmt, w, h, {src, src_size}, {dst, dst_size}, pitch);
}

int AcquireTextureSlot() {
    for (int i = 0; i < g_tex_count; i++) {
        if (g_tex[i] == nullptr) return i;
    }
    if (g_tex_count >= kMaxTex) return -1;
    return g_tex_count++;
}

int AFP_CB Cb_TexCreate(void* ctx, unsigned int w, unsigned int h, int fmt, int a5, int a6,
                        int a7) {
    (void)ctx;
    (void)a5;
    (void)a6;
    (void)a7;
    int const id = AcquireTextureSlot();
    if (id < 0 || (g_dev == nullptr)) return id;
    IDirect3DTexture9* t = nullptr;
    HRESULT const hr =
        g_dev->CreateTexture((w != 0U) ? w : 1, (h != 0U) ? h : 1, 1, D3DUSAGE_DYNAMIC,
                             D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &t, nullptr);
    g_tex[id] = SUCCEEDED(hr) ? t : nullptr;
    if (g_frame < 2) {
        LOG("DDR-R", "TexCreate id=%d %ux%u fmt=%d -> %p hr=0x%lx", id, w, h, fmt, (void*)g_tex[id],
            hr);
    }
    return id;
}

void ReleaseTextureSlot(int id) {
    if (id >= 0 && id < kMaxTex && (g_tex[id] != nullptr)) {
        g_tex[id]->Release();
        g_tex[id] = nullptr;
    }
}

void AFP_CB Cb_TexDestroy(int id) {
    ReleaseTextureSlot(id);
}

namespace {

void PutArgb(uint8_t* d, uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    d[0] = b;
    d[1] = g;
    d[2] = r;
    d[3] = a;
}

void DecodeTexRect(int fmt, const uint8_t* src, int w, int h, uint8_t* dstBase, LONG pitch) {
    switch (fmt) {
    case 14: {
        for (int row = 0; row < h; row++) {
            const uint8_t* s = src + ((size_t)row * w * 3);
            uint8_t* d = dstBase + ((size_t)row * pitch);
            for (int i = 0; i < w; i++, s += 3, d += 4)
                PutArgb(d, 0xFF, s[2], s[1], s[0]);
        }
        break;
    }
    case 16:
    case 32: {
        for (int row = 0; row < h; row++)
            memcpy(dstBase + ((size_t)row * pitch), src + ((size_t)row * w * 4), (size_t)w * 4);
        break;
    }
    case 31: {
        for (int row = 0; row < h; row++) {
            const uint8_t* s = src + ((size_t)row * w * 2);
            uint8_t* d = dstBase + ((size_t)row * pitch);
            for (int i = 0; i < w; i++, s += 2, d += 4) {
                auto p = (uint16_t)(s[0] | (s[1] << 8));
                auto r = (uint8_t)(((p >> 11) & 0x1F) * 255 / 31);
                auto g = (uint8_t)(((p >> 5) & 0x3F) * 255 / 63);
                auto b = (uint8_t)((p & 0x1F) * 255 / 31);
                PutArgb(d, 0xFF, r, g, b);
            }
        }
        break;
    }
    case 22:
        DecodeDxtToRect(src, w, h, dstBase, pitch, false);
        break;
    case 26:
        DecodeDxtToRect(src, w, h, dstBase, pitch, true);
        break;
    default:
        if (g_frame < 2) LOG("DDR-R", "TexUpload UNHANDLED fmt=%d (%dx%d)", fmt, w, h);
        break;
    }
}

}

void AFP_CB Cb_TexUpload(int id, int fmt, intptr_t a3, intptr_t a4, int x, int y, int w, int h,
                         void* pixels) {
    (void)a3;
    (void)a4;
    if (id < 0 || id >= kMaxTex || (g_tex[id] == nullptr) || (pixels == nullptr)) return;
    int const df = DumpFrame();
    if (g_frame < 2 || (df >= 0 && g_frame >= df - 2 && g_frame <= df + 2))
        LOG("DDR-R", "TexUpload f%d id=%d fmt=%d %dx%d @(%d,%d)", g_frame, id, fmt, w, h, x, y);

    const auto* src = (const uint8_t*)pixels;
    RECT const rc = {x, y, x + w, y + h};
    D3DLOCKED_RECT lr;
    if (FAILED(g_tex[id]->LockRect(0, &lr, &rc, 0))) return;
    auto* dstBase = (uint8_t*)lr.pBits;

    DecodeTexRect(fmt, src, w, h, dstBase, lr.Pitch);
    g_tex[id]->UnlockRect(0);
}

constexpr size_t kAfpAllocAlign = 16;

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

bool g_filter_on = false;
float g_filter_dh = 0.0F, g_filter_ds = 0.0F, g_filter_dl = 0.0F;

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

void DumpAtlases() {
    using SaveTexFn = HRESULT(WINAPI*)(LPCSTR, DWORD, IDirect3DBaseTexture9*, const PALETTEENTRY*);
    static SaveTexFn pSave = nullptr;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        const char* dlls[] = {"d3dx9_43.dll", "d3dx9_42.dll", "d3dx9_41.dll", "d3dx9_40.dll",
                              "d3dx9_39.dll", "d3dx9_36.dll", "d3dx9_33.dll", "d3dx9_30.dll"};
        for (const char* n : dlls) {
            HMODULE m = LoadLibraryA(n);
            if (m != nullptr) {
                pSave = (SaveTexFn)GetProcAddress(m, "D3DXSaveTextureToFileA");
                if (pSave != nullptr) break;
            }
        }
    }
    if (pSave == nullptr) {
        LOG("DDR-R", "DumpAtlases: D3DXSaveTextureToFileA unresolved");
        return;
    }
    for (int i = 0; i < g_tex_count && i < kMaxTex; i++) {
        if (g_tex[i] == nullptr) continue;
        char path[256];
        snprintf(path, sizeof(path), "screenshots/atlas_%02d.png", i);
        HRESULT const hr = pSave(path, 3, g_tex[i], nullptr);
        LOG("DDR-R", "DumpAtlases: tex %d -> %s hr=0x%lx", i, path, hr);
    }
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

int g_blend_mode = 0;

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

TexBindResolver g_tex_bind = nullptr;
BitmapQuery g_bitmap_query = nullptr;

int AFP_CB Cb_IdentityTexBind(unsigned id) {
    return (int)id;
}

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
    int const n = count < 16384 ? count : 16384;
    g_max_count = std::max(count, g_max_count);
    if (count > 16384) g_trunc_count++;
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

    static Vtx buf[16384];
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

intptr_t Cb_Noop() {
    return 0;
}

constexpr size_t kRenderParamsSlots = 0x140 / 8;
constexpr size_t kAfpuConfigSlots = 0x80 / 8;

constexpr size_t kSlotInitFrame = 1;
constexpr size_t kSlotFinishFrame = 2;
constexpr size_t kSlotSetMask = 3;
constexpr size_t kSlotSetBlend = 4;
constexpr size_t kSlotSetPriority = 5;
constexpr size_t kSlotSetFilter = 6;
constexpr size_t kSlotDrawPrimitive = 7;
constexpr size_t kSlotDrawShape = 8;
constexpr size_t kSlotLoadMatrix = 9;
constexpr size_t kSlotLoadMatrix44 = 10;
constexpr size_t kSlotLoadProj44 = 11;
constexpr size_t kSlotGetScreenSize = 12;
constexpr size_t kSlotGetNearFar = 13;
constexpr size_t kSlotSetDrawRect = 14;
constexpr size_t kSlotOptionalFirst = 15;
constexpr size_t kSlotGetShapeId = 16;
constexpr size_t kSlotGetShapeRect = 17;
constexpr size_t kSlotOptionalLast = 34;
constexpr size_t kSlotReserved = 35;
constexpr size_t kSlotAlloc = 36;
constexpr size_t kSlotRealloc = 37;
constexpr size_t kSlotFree = 38;

constexpr size_t kSlotTexCreate = 0;
constexpr size_t kSlotTexDestroy = 1;
constexpr size_t kSlotTexUpload = 2;
constexpr size_t kSlotAfpuAlloc = 4;
constexpr size_t kSlotAfpuRealloc = 5;
constexpr size_t kSlotAfpuFree = 6;
constexpr size_t kSlotAfpuNear = 7;

uint8_t g_render_params[kRenderParamsSlots * Support::kEngineSlot];
uint8_t g_afpu_config[kAfpuConfigSlots * Support::kEngineSlot];

void PutSlot(uint8_t* base, size_t slot, void* value) {
    memcpy(base + Support::SlotOffset(slot), static_cast<const void*>(&value), sizeof(value));
}

template <class F> void Put(uint8_t* base, size_t slot, F fn) {
    PutSlot(base, slot, reinterpret_cast<void*>(fn));
}

void BuildStructs(bool legacy_draw_primitive) {
    memset(g_render_params, 0, sizeof(g_render_params));
    *reinterpret_cast<uint32_t*>(g_render_params) = 0x200;
    for (size_t slot = 1; slot < kRenderParamsSlots; slot++)
        Put(g_render_params, slot, Cb_Noop);
    Put(g_render_params, kSlotInitFrame, Cb_InitFrame);
    Put(g_render_params, kSlotFinishFrame, Cb_FinishFrame);
    Put(g_render_params, kSlotSetMask, Cb_SetMask);
    Put(g_render_params, kSlotSetBlend, Cb_SetBlend);
    Put(g_render_params, kSlotSetPriority, Cb_SetPriority);
    Put(g_render_params, kSlotSetFilter, Cb_SetFilter);
    if (legacy_draw_primitive) {
        Put(g_render_params, kSlotDrawPrimitive, Cb_DrawPrimitiveLegacy);
        Put(g_render_params, kSlotOptionalFirst, Cb_GetBitmapInfo);
        Put(g_render_params, kSlotGetShapeId, Cb_GetShapeId);
        Put(g_render_params, kSlotGetShapeRect, Cb_GetShapeRect);
        g_tex_bind = Cb_IdentityTexBind;
    } else {
        Put(g_render_params, kSlotDrawPrimitive, Cb_DrawPrimitive);
    }
    Put(g_render_params, kSlotDrawShape, Cb_DrawShape);
    Put(g_render_params, kSlotLoadMatrix, Cb_LoadMatrix);
    Put(g_render_params, kSlotLoadMatrix44, Cb_LoadMatrix44);
    Put(g_render_params, kSlotLoadProj44, Cb_LoadProj44);
    Put(g_render_params, kSlotGetScreenSize, Cb_GetScreenSize);
    Put(g_render_params, kSlotGetNearFar, Cb_GetNearFar);
    Put(g_render_params, kSlotSetDrawRect, Cb_SetDrawRect);
    if (Support::EnvFlag("DDR_DEFAULT_CB")) {
        for (size_t slot = kSlotOptionalFirst; slot <= kSlotOptionalLast; slot++)
            PutSlot(g_render_params, slot, nullptr);
        LOG("DDR-R", "DDR_DEFAULT_CB: nulled render_params slots %zu..%zu (afp defaults)",
            kSlotOptionalFirst, kSlotOptionalLast);
    }
    PutSlot(g_render_params, kSlotReserved, nullptr);
    Put(g_render_params, kSlotAlloc, Cb_Alloc);
    Put(g_render_params, kSlotRealloc, Cb_Realloc);
    Put(g_render_params, kSlotFree, Cb_Free);

    memset(g_afpu_config, 0, sizeof(g_afpu_config));
    Put(g_afpu_config, kSlotTexCreate, Cb_TexCreate);
    Put(g_afpu_config, kSlotTexDestroy, Cb_TexDestroy);
    Put(g_afpu_config, kSlotTexUpload, Cb_TexUpload);
    Put(g_afpu_config, kSlotAfpuAlloc, Cb_Alloc);
    Put(g_afpu_config, kSlotAfpuRealloc, Cb_Realloc);
    Put(g_afpu_config, kSlotAfpuFree, Cb_Free);
    auto* afpu_planes =
        reinterpret_cast<float*>(g_afpu_config + Support::SlotOffset(kSlotAfpuNear));
    afpu_planes[0] = 1.0F;
    afpu_planes[1] = 9999.0F;
}

}

namespace {
void SetupOrthoProjection() {
    memset(&g_proj, 0, sizeof(g_proj));
    g_proj._11 = 2.0F / (float)g_w;
    g_proj._22 = -2.0F / (float)g_h;
    g_proj._33 = 1.0F;
    g_proj._41 = -1.0F;
    g_proj._42 = 1.0F;
    g_proj._44 = 1.0F;
    g_have_proj = true;
}
}

void Init(IDirect3DDevice9* device, int screen_w, int screen_h, bool legacy_draw_primitive) {
    g_dev = device;
    g_w = screen_w;
    g_h = screen_h;
    IdentityM(g_proj);
    IdentityM(g_world);
    SetupOrthoProjection();
    BuildStructs(legacy_draw_primitive);
    LOG("DDR-R", "render backend init %dx%d dev=%p (ortho proj, draw_primitive=%s)", g_w, g_h,
        (void*)g_dev, legacy_draw_primitive ? "legacy 9-arg" : "unified 4-arg");
}

void* RenderParams() {
    return g_render_params;
}
void* AfpuConfig() {
    return g_afpu_config;
}
void SetScreenSize(int w, int h) {
    g_w = w;
    g_h = h;
}
void SetBitmapQuery(BitmapQuery fn) {
    g_bitmap_query = fn;
}

int CreateTextureRgba(int w, int h, const unsigned char* pixels, size_t pixel_bytes) {
    if ((g_dev == nullptr) || w <= 0 || h <= 0 || pixels == nullptr) return -1;
    if (pixel_bytes < (size_t)w * (size_t)h * 4) return -1;
    int const slot = AcquireTextureSlot();
    if (slot < 0) return -1;

    IDirect3DTexture9* t = nullptr;
    if (FAILED(g_dev->CreateTexture((UINT)w, (UINT)h, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
                                    D3DPOOL_DEFAULT, &t, nullptr))) {
        return -1;
    }
    D3DLOCKED_RECT lr;
    if (FAILED(t->LockRect(0, &lr, nullptr, 0))) {
        t->Release();
        return -1;
    }
    for (int row = 0; row < h; row++) {
        memcpy(static_cast<uint8_t*>(lr.pBits) + ((size_t)row * lr.Pitch),
               pixels + ((size_t)row * (size_t)w * 4), (size_t)w * 4);
    }
    t->UnlockRect(0);

    g_tex[slot] = t;
    LOG("DDR-R", "CreateTextureRgba id=%d %dx%d", slot, w, h);
    return slot;
}

namespace {

IDirect3DTexture9* TextureAt(int slot) {
    if (slot < 0 || slot >= g_tex_count) return nullptr;
    return g_tex[slot];
}

void UvBias(IDirect3DTexture9* tex, float& du, float& dv) {
    du = 0.0F;
    dv = 0.0F;
    D3DSURFACE_DESC desc;
    if (tex == nullptr || FAILED(tex->GetLevelDesc(0, &desc))) return;
    if (desc.Width != 0) du = 0.001F / (float)desc.Width;
    if (desc.Height != 0) dv = 0.001F / (float)desc.Height;
}

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

    static Vtx buf[16384];
    int const n = std::min(index_count - (index_count % 3), 16384);
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

void DestroyTexture(int id) {
    ReleaseTextureSlot(id);
}

void SetTexBindResolver(TexBindResolver fn) {
    g_tex_bind = fn;
}
int DrawCount() {
    return g_draw_count;
}
void ResetDrawCount() {
    g_draw_count = 0;
}

}
