#include "gpu_context.h"
#include <d3d9.h>
#include <intsafe.h>
#include <cstdint>
#include "formats/dxt_decode.h"
#include "render_backend.h"
#include "support/log.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <wincodec.h>
#include <winnls.h>
#include <wtypesbase.h>

namespace AfpD3D9 {

int __cdecl TexCreate(void* ctx, unsigned int width, unsigned int height) {
    (void)ctx;
    LOG("AfpD3D9", "TexCreate(ctx=%p, %ux%u) device=%p", ctx, width, height, g_gpu.device);
    if (g_gpu.device == nullptr) return -1;
    if (g_gpu.next_tex_slot >= kTexSlotCount) {
        LOG("AfpD3D9", "TexCreate: slot table FULL (%d/%d) - texture DROPPED (%ux%u)",
            g_gpu.next_tex_slot, kTexSlotCount, width, height);
        return -1;
    }

    IDirect3DTexture9* tex = nullptr;
    HRESULT const hr = g_gpu.device->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC,
                                                   D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, nullptr);
    if (FAILED(hr)) {
        LOG("AfpD3D9", "TexCreate(%ux%u) FAILED (0x%08lx)", width, height, hr);
        return -1;
    }

    int const slot = g_gpu.next_tex_slot++;
    g_gpu.textures[slot] = tex;
    g_gpu.texture_dims_w[slot] = (int)width;
    g_gpu.texture_dims_h[slot] = (int)height;

    unsigned int mag = 0;
    unsigned int min = 0;
    if (g_gpu.atlas_filter_queue_head < g_gpu.atlas_filter_queue.size()) {
        auto& af = g_gpu.atlas_filter_queue[g_gpu.atlas_filter_queue_head++];
        mag = af.mag;
        min = af.min;
    }
    g_gpu.texture_mag_filter[slot] = mag;
    g_gpu.texture_min_filter[slot] = min;

    if (g_gpu.texture_width == 0) {
        g_gpu.texture_width = static_cast<int>(width);
        g_gpu.texture_height = static_cast<int>(height);
    }

    if (g_gpu.on_texture_created != nullptr) g_gpu.on_texture_created();

    LOG("AfpD3D9", "TexCreate(%ux%u) -> slot %d (tex=%p) filter=mag:%u/min:%u", width, height, slot,
        tex, mag, min);
    return slot;
}

namespace {

struct WicDecodeTarget {
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* conv = nullptr;
    bool com_inited = false;

    WicDecodeTarget() = default;
    WicDecodeTarget(const WicDecodeTarget&) = delete;
    WicDecodeTarget& operator=(const WicDecodeTarget&) = delete;
    WicDecodeTarget(WicDecodeTarget&&) = delete;
    WicDecodeTarget& operator=(WicDecodeTarget&&) = delete;
    ~WicDecodeTarget() {
        if (conv != nullptr) conv->Release();
        if (frame != nullptr) frame->Release();
        if (decoder != nullptr) decoder->Release();
        if (factory != nullptr) factory->Release();
        if (com_inited) CoUninitialize();
    }
};

std::wstring WidenUtf8Path(const std::string& path) {
    int const wn = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wpath;
    if (wn > 0) {
        wpath.resize(wn - 1);
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wn);
    }
    return wpath;
}

bool DecodeImageBgra(WicDecodeTarget& t, const std::string& path, std::vector<uint8_t>& bgra,
                     UINT& w, UINT& h) {
    std::wstring const wpath = WidenUtf8Path(path);
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&t.factory)))) {
        LOG("AfpD3D9", "LoadExternalImageSlot: WIC factory create failed");
        return false;
    }
    if (FAILED(t.factory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnDemand, &t.decoder))) {
        LOG("AfpD3D9", "LoadExternalImageSlot: cannot open '%s'", path.c_str());
        return false;
    }
    if (FAILED(t.decoder->GetFrame(0, &t.frame))) return false;

    t.frame->GetSize(&w, &h);
    if (w == 0 || h == 0) return false;

    if (FAILED(t.factory->CreateFormatConverter(&t.conv))) return false;
    if (FAILED(t.conv->Initialize(t.frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
                                  nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
        return false;
    }
    bgra.assign((size_t)w * h * 4, 0);
    WICRect const rc{0, 0, (INT)w, (INT)h};
    return !FAILED(t.conv->CopyPixels(&rc, w * 4, (UINT)bgra.size(), bgra.data()));
}

}

int LoadExternalImageSlot(const std::string& path, int& out_w, int& out_h) {
    out_w = out_h = 0;
    if (g_gpu.device == nullptr) {
        LOG("AfpD3D9", "LoadExternalImageSlot: no device");
        return -1;
    }

    WicDecodeTarget t;
    t.com_inited = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    std::vector<uint8_t> bgra;
    UINT w = 0;
    UINT h = 0;
    if (!DecodeImageBgra(t, path, bgra, w, h)) return -1;

    int const slot = TexCreate(nullptr, w, h);
    if (slot < 0) return -1;
    TexUpload((unsigned)slot, 0x10, w, h, 0, 0, (int)w, (int)h, bgra.data());

    out_w = (int)w;
    out_h = (int)h;
    LOG("AfpD3D9", "LoadExternalImageSlot('%s') -> slot %d (%ux%u)", path.c_str(), slot, w, h);
    return slot;
}

void EnqueueAtlasFilter(unsigned int mag_d3d, unsigned int min_d3d) {
    g_gpu.atlas_filter_queue.push_back({.mag = mag_d3d, .min = min_d3d});
}

void ClearAtlasFilterQueue() {
    g_gpu.atlas_filter_queue.clear();
    g_gpu.atlas_filter_queue_head = 0;
}

void MarkPersistentBoundary() {
    g_gpu.persistent_tex_high_water = g_gpu.next_tex_slot;
    LOG("AfpD3D9", "MarkPersistentBoundary: %d persistent textures locked in",
        g_gpu.persistent_tex_high_water - 1);
}

IDirect3DTexture9* ResolveTexture(uint32_t tex_ref) {
    if (tex_ref == 0) return nullptr;
    uint32_t slot_id = 0;
    if ((tex_ref & 0x78000000U) == 0x08000000U && (g_gpu.afpu_get_tex_slot != nullptr)) {
        slot_id = g_gpu.afpu_get_tex_slot(tex_ref);
    } else {
        slot_id = (tex_ref & 0xFFFFU) + 1;
    }
    if (slot_id == 0 || slot_id >= kTexSlotCount) return nullptr;
    return g_gpu.textures[slot_id];
}

bool ReadTexturePixels(IDirect3DTexture9* tex, std::vector<uint8_t>& out, int& out_w, int& out_h) {
    out.clear();
    out_w = out_h = 0;
    if (tex == nullptr) return false;

    D3DSURFACE_DESC desc{};
    if (FAILED(tex->GetLevelDesc(0, &desc))) return false;
    if (desc.Width == 0 || desc.Height == 0) return false;
    if (desc.Format != D3DFMT_A8R8G8B8) {
        LOG("AfpD3D9", "ReadTexturePixels: unsupported format 0x%x (want A8R8G8B8)",
            (unsigned)desc.Format);
        return false;
    }

    D3DLOCKED_RECT locked{};
    HRESULT const hr = tex->LockRect(0, &locked, nullptr, D3DLOCK_READONLY);
    if (FAILED(hr)) {
        LOG("AfpD3D9", "ReadTexturePixels: LockRect failed 0x%08lx (w=%u h=%u)", (unsigned long)hr,
            desc.Width, desc.Height);
        return false;
    }

    const int row_bytes = (int)desc.Width * 4;
    out.resize((size_t)row_bytes * desc.Height);
    const auto* src = (const uint8_t*)locked.pBits;
    uint8_t* dst = out.data();
    for (UINT y = 0; y < desc.Height; y++) {
        std::memcpy(dst, src, (size_t)row_bytes);
        src += locked.Pitch;
        dst += row_bytes;
    }
    tex->UnlockRect(0);
    out_w = (int)desc.Width;
    out_h = (int)desc.Height;
    return true;
}

int NextSlot() {
    return g_gpu.next_tex_slot;
}

IDirect3DTexture9* GetTexture(int slot) {
    if (slot <= 0 || slot >= kTexSlotCount) return nullptr;
    return g_gpu.textures[slot];
}

bool GetTextureSize(int slot, int& w, int& h) {
    w = h = 0;
    IDirect3DTexture9* tex = GetTexture(slot);
    if (tex == nullptr) return false;
    D3DSURFACE_DESC d{};
    if (FAILED(tex->GetLevelDesc(0, &d))) return false;
    w = (int)d.Width;
    h = (int)d.Height;
    return true;
}

void SetNextSlot(int slot) {
    slot = std::max(slot, 1);
    slot = std::min(slot, kTexSlotCount);
    g_gpu.next_tex_slot = slot;
}

void SetQproDrawProbe(bool on) {
    g_gpu.qpro_draw_probe = on;
}

void SetHandRenderShift(bool on) {
    g_gpu.afp_render_x_offset = on ? 0.30F * (float)g_gpu.screen_w : 0.0F;
}
float RenderXOffset() {
    return g_gpu.afp_render_x_offset;
}

void SetHsvScopeRect(float umin, float umax, float vmin, float vmax) {
    g_gpu.hsv_scope_rect[0] = umin;
    g_gpu.hsv_scope_rect[1] = umax;
    g_gpu.hsv_scope_rect[2] = vmin;
    g_gpu.hsv_scope_rect[3] = vmax;
}

void ResetHsvScopeRect() {
    g_gpu.hsv_scope_rect[0] = 0.0F;
    g_gpu.hsv_scope_rect[1] = 1.0F;
    g_gpu.hsv_scope_rect[2] = 0.0F;
    g_gpu.hsv_scope_rect[3] = 1.0F;
    g_gpu.hsv_scope2_rect[0] = g_gpu.hsv_scope2_rect[1] = 0.0F;
    g_gpu.hsv_scope2_rect[2] = g_gpu.hsv_scope2_rect[3] = 0.0F;
}

void SetHsvScopeRect2(float umin, float umax, float vmin, float vmax) {
    g_gpu.hsv_scope2_rect[0] = umin;
    g_gpu.hsv_scope2_rect[1] = umax;
    g_gpu.hsv_scope2_rect[2] = vmin;
    g_gpu.hsv_scope2_rect[3] = vmax;
}

void ResetAllTextures() {
    if (g_gpu.device != nullptr) g_gpu.device->SetTexture(0, nullptr);
    int released = 0;
    for (int i = g_gpu.persistent_tex_high_water; i < kTexSlotCount; i++) {
        if (g_gpu.textures[i] != nullptr) {
            g_gpu.textures[i]->Release();
            g_gpu.textures[i] = nullptr;
            released++;
        }
    }
    g_gpu.current_texture = nullptr;
    LOG("AfpD3D9", "ResetAllTextures: released %d scene textures (kept %d persistent)", released,
        g_gpu.persistent_tex_high_water - 1);
}

void __cdecl TexDestroy(unsigned int tex_id) {
    if (tex_id > 0 && tex_id < kTexSlotCount && (g_gpu.textures[tex_id] != nullptr)) {
        IDirect3DTexture9* tex = g_gpu.textures[tex_id];
        if (g_gpu.current_texture == tex) g_gpu.current_texture = nullptr;
        if (g_gpu.device != nullptr) g_gpu.device->SetTexture(0, nullptr);
        tex->Release();
        g_gpu.textures[tex_id] = nullptr;
        LOG("AfpD3D9", "TexDestroy(%u)", tex_id);
    }
}

namespace {

int FormatBpp(unsigned int format, bool& is_dxt) {
    is_dxt = false;
    switch (format) {
    case 0x01:
    case 0x1E:
        return 1;
    case 0x0E:
        return 3;
    case 0x1F:
        return 2;
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
        is_dxt = true;
        return 1;
    case 0x10:
    case 0x20:
    default:
        return 4;
    }
}

void ConvertRows(BYTE* dst, const BYTE* src, int src_w, int src_h, int bpp, LONG pitch) {
    for (int y = 0; y < src_h; y++) {
        if (bpp == 4) {
            memcpy(dst, src, (size_t)src_w * 4);
        } else if (bpp == 3) {
            for (int x = 0; x < src_w; x++) {
                dst[(x * 4) + 0] = src[(x * 3) + 2];
                dst[(x * 4) + 1] = src[(x * 3) + 1];
                dst[(x * 4) + 2] = src[(x * 3) + 0];
                dst[(x * 4) + 3] = 0xFF;
            }
        } else if (bpp == 2) {
            for (int x = 0; x < src_w; x++) {
                uint16_t const px = ((const uint16_t*)src)[x];
                dst[(x * 4) + 0] = (px & 0x0F) * 17;
                dst[(x * 4) + 1] = ((px >> 4) & 0x0F) * 17;
                dst[(x * 4) + 2] = ((px >> 8) & 0x0F) * 17;
                dst[(x * 4) + 3] = ((px >> 12) & 0x0F) * 17;
            }
        } else {
            for (int x = 0; x < src_w; x++) {
                BYTE const v = src[x];
                dst[(x * 4) + 0] = v;
                dst[(x * 4) + 1] = v;
                dst[(x * 4) + 2] = v;
                dst[(x * 4) + 3] = v;
            }
        }
        src += (size_t)src_w * bpp;
        dst += pitch;
    }
}

}

void __cdecl TexUpload(unsigned int tex_id, unsigned int format,
                       [[maybe_unused]] unsigned int width, [[maybe_unused]] unsigned int height,
                       int x_off, int y_off, int src_w, int src_h, void* pixel_data) {
    if (tex_id == 0 || tex_id >= kTexSlotCount || (g_gpu.textures[tex_id] == nullptr) ||
        (pixel_data == nullptr))
        return;

    IDirect3DTexture9* tex = g_gpu.textures[tex_id];
    D3DLOCKED_RECT lr;
    HRESULT const hr = tex->LockRect(0, &lr, nullptr, 0);
    if (FAILED(hr)) {
        LOG("AfpD3D9", "TexUpload: LockRect failed");
        return;
    }

    bool is_dxt = false;
    int const bpp = FormatBpp(format, is_dxt);

    auto* src = (BYTE*)pixel_data;
    BYTE* dst = (BYTE*)lr.pBits + ((size_t)y_off * lr.Pitch) + ((size_t)x_off * 4);

    static int upload_count = 0;
    static int per_slot_count[kTexSlotCount] = {};
    upload_count++;
    per_slot_count[tex_id]++;
    if (per_slot_count[tex_id] <= 3 || upload_count % 100 == 0) {
        LOG("AfpD3D9",
            "TexUpload #%d (slot %u upload %d): id=%u fmt=0x%x off=(%d,%d) src=(%dx%d) bpp=%d "
            "pitch=%ld",
            upload_count, tex_id, per_slot_count[tex_id], tex_id, format, x_off, y_off, src_w,
            src_h, bpp, lr.Pitch);
    }

    if (is_dxt) {
        const size_t dxt_src = Dxt::EncodedSize(format, src_w, src_h);
        const size_t dxt_dst = ((size_t)(src_h - 1) * lr.Pitch) + ((size_t)src_w * 4);
        Dxt::Decompress(format, src_w, src_h, {src, dxt_src}, {dst, dxt_dst}, (int)lr.Pitch);
    } else {
        ConvertRows(dst, src, src_w, src_h, bpp, lr.Pitch);
    }

    tex->UnlockRect(0);
    g_gpu.current_texture = tex;
}

}
