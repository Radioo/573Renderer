#include <d3d9.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>
#include "afp_ddr_render_internal.h"
#include "support/engine_abi.h"
#include "support/log.h"
#include "support/module_handle.h"
#include "formats/dxt_decode.h"

namespace DdrRender {
namespace {

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

void ReleaseTextureSlot(int id) {
    if (id >= 0 && id < kMaxTex && (g_tex[id] != nullptr)) {
        g_tex[id]->Release();
        g_tex[id] = nullptr;
    }
}

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

void AFP_CB Cb_TexDestroy(int id) {
    ReleaseTextureSlot(id);
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

void DumpAtlases() {
    using SaveTexFn = HRESULT(WINAPI*)(LPCSTR, DWORD, IDirect3DBaseTexture9*, const PALETTEENTRY*);
    static SaveTexFn pSave = nullptr;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        const char* dlls[] = {"d3dx9_43.dll", "d3dx9_42.dll", "d3dx9_41.dll", "d3dx9_40.dll",
                              "d3dx9_39.dll", "d3dx9_36.dll", "d3dx9_33.dll", "d3dx9_30.dll"};
        static Support::ModuleHandle s_d3dx_mod;
        for (const char* n : dlls) {
            Support::ModuleHandle m = Support::LoadModule(n);
            if (m == nullptr) continue;
            pSave = (SaveTexFn)GetProcAddress(m.get(), "D3DXSaveTextureToFileA");
            if (pSave != nullptr) {
                s_d3dx_mod = std::move(m);
                break;
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

void DestroyTexture(int id) {
    ReleaseTextureSlot(id);
}

}
