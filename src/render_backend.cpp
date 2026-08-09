#include "render_backend.h"
#include "support/log.h"
#include "support/module_handle.h"
#include "gpu_context.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <d3d9.h>
#include <d3d9caps.h>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>

namespace {

bool CreateDeviceForWindow(D3D9State& st) {
    IDirect3D9Ex* d3dex = nullptr;
    HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3dex);
    if (FAILED(hr) || (d3dex == nullptr)) {
        LOG("D3D9", "Direct3DCreate9Ex failed (hr=0x%08lx), falling back to D3D9", hr);
        st.d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (st.d3d == nullptr) {
            LOG("D3D9", "Direct3DCreate9 also failed");
            return false;
        }
    } else {
        st.d3d = d3dex;
        LOG("D3D9", "Using D3D9Ex");
    }

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = st.width;
    pp.BackBufferHeight = st.height;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    pp.hDeviceWindow = st.hwnd;
    pp.EnableAutoDepthStencil = FALSE;

    if (d3dex != nullptr) {
        IDirect3DDevice9Ex* deviceEx = nullptr;
        hr = d3dex->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, st.hwnd,
                                   D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
                                   &pp, nullptr, &deviceEx);
        if (FAILED(hr)) {
            hr =
                d3dex->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, st.hwnd,
                                      D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, nullptr, &deviceEx);
        }
        st.device = deviceEx;
    } else {
        hr = st.d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, st.hwnd,
                                  D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
                                  &pp, &st.device);
        if (FAILED(hr)) {
            hr = st.d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, st.hwnd,
                                      D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &st.device);
        }
    }

    if (FAILED(hr) || (st.device == nullptr)) {
        LOG("D3D9", "Failed to create device (hr=0x%08lx)", hr);
        st.d3d->Release();
        st.d3d = nullptr;
        return false;
    }

    LOG("D3D9", "Device created (%dx%d, Ex=%d)", st.width, st.height, d3dex != nullptr);
    return true;
}

bool CreateRenderTargets(D3D9State& st) {
    HRESULT hr = 0;
    hr = st.device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &st.backbuffer);
    if (FAILED(hr) || (st.backbuffer == nullptr)) {
        LOG("D3D9", "GetBackBuffer failed (hr=0x%08lx)", hr);
        return false;
    }

    hr = st.device->CreateRenderTarget(st.width, st.height, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0,
                                       TRUE, &st.offscreen_rt, nullptr);
    if (FAILED(hr)) {
        LOG("D3D9", "CreateRenderTarget failed (hr=0x%08lx)", hr);
        return false;
    }

    hr =
        st.device->CreateDepthStencilSurface(st.width, st.height, D3DFMT_D24S8, D3DMULTISAMPLE_NONE,
                                             0, TRUE, &st.depth_stencil, nullptr);
    if (FAILED(hr)) {
        LOG("D3D9", "CreateDepthStencilSurface failed (hr=0x%08lx)", hr);
        return false;
    }

    LOG("D3D9", "Offscreen RT (%p) + DS (%p) + st.backbuffer (%p) ready", st.offscreen_rt,
        st.depth_stencil, st.backbuffer);
    return true;
}

}

bool D3D9State::Init(HWND window) {
    hwnd = window;
    if (!CreateDeviceForWindow(*this)) return false;
    if (!CreateRenderTargets(*this)) return false;
    return true;
}

namespace {
IDirect3DSurface9* g_readback_sysmem;
}
namespace {
int g_readback_w;
}
namespace {
int g_readback_h;
}

void D3D9State::Shutdown() {
    if (afp_texture != nullptr) {
        afp_texture->Release();
        afp_texture = nullptr;
    }
    if (depth_stencil != nullptr) {
        depth_stencil->Release();
        depth_stencil = nullptr;
    }
    if (offscreen_rt != nullptr) {
        offscreen_rt->Release();
        offscreen_rt = nullptr;
    }
    if (backbuffer != nullptr) {
        backbuffer->Release();
        backbuffer = nullptr;
    }
    if (g_readback_sysmem != nullptr) {
        g_readback_sysmem->Release();
        g_readback_sysmem = nullptr;
    }
    g_readback_w = g_readback_h = 0;
    if (device != nullptr) {
        device->Release();
        device = nullptr;
    }
    if (d3d != nullptr) {
        d3d->Release();
        d3d = nullptr;
    }
}

void D3D9State::BeginFrame() const {
    if (device == nullptr) return;

    HRESULT const bshr = device->BeginScene();

    HRESULT srthr = D3D_OK;
    HRESULT sdshr = D3D_OK;
    if (offscreen_rt != nullptr) srthr = device->SetRenderTarget(0, offscreen_rt);
    if (depth_stencil != nullptr) sdshr = device->SetDepthStencilSurface(depth_stencil);
    if (FAILED(bshr) || FAILED(srthr) || FAILED(sdshr)) {
        static int bff = 0;
        if (bff++ < 40) {
            LOG("D3D9",
                "BeginFrame FAIL: BeginScene=0x%08lx SetRT=0x%08lx SetDS=0x%08lx (devstate?)",
                (unsigned long)bshr, (unsigned long)srthr, (unsigned long)sdshr);
        }
    }

    device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, clear_color,
                  1.0F, 0);
}

namespace {
std::string g_pending_screenshot_path;
}

void D3D9State_RequestScreenshot(const char* path) {
    g_pending_screenshot_path = (path != nullptr) ? path : "";
}

void D3D9State::EndFrame() const {
    if (device == nullptr) return;

    if (backbuffer != nullptr) device->SetRenderTarget(0, backbuffer);

    if ((offscreen_rt != nullptr) && (backbuffer != nullptr)) {
        HRESULT const hr =
            device->StretchRect(offscreen_rt, nullptr, backbuffer, nullptr, D3DTEXF_LINEAR);
        if (FAILED(hr)) {
            static int log_count = 0;
            if (log_count++ < 3)
                LOG("D3D9", "StretchRect offscreen->backbuffer failed (hr=0x%08lx)", hr);
        }
    }

    DrawCropOverlay();

    if (!g_pending_screenshot_path.empty()) {
        SaveBackBufferToFile(g_pending_screenshot_path.c_str());
        g_pending_screenshot_path.clear();
    }

    HRESULT const eshr = device->EndScene();
    HRESULT const phr = device->Present(nullptr, nullptr, nullptr, nullptr);
    if (FAILED(eshr) || FAILED(phr)) {
        static int eff = 0;
        if (eff++ < 40) {
            LOG("D3D9", "EndFrame FAIL: EndScene=0x%08lx Present=0x%08lx (devstate?)",
                (unsigned long)eshr, (unsigned long)phr);
        }
    }
}

bool D3D9State::SaveBackBufferToFile(const char* path) const {
    if ((device == nullptr) || (backbuffer == nullptr)) return false;

    using D3DXSaveSurfaceToFileAFn =
        HRESULT(WINAPI*)(LPCSTR, DWORD, IDirect3DSurface9*, const PALETTEENTRY*, const RECT*);

    static D3DXSaveSurfaceToFileAFn pSave = nullptr;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        const char* dlls[] = {
            "d3dx9_43.dll", "d3dx9_42.dll", "d3dx9_41.dll", "d3dx9_40.dll", "d3dx9_39.dll",
            "d3dx9_38.dll", "d3dx9_37.dll", "d3dx9_36.dll", "d3dx9_35.dll", "d3dx9_34.dll",
            "d3dx9_33.dll", "d3dx9_32.dll", "d3dx9_31.dll", "d3dx9_30.dll", "d3dx9_29.dll",
            "d3dx9_28.dll", "d3dx9_27.dll", "d3dx9_26.dll", "d3dx9_25.dll", "d3dx9_24.dll",
        };
        static Support::ModuleHandle s_d3dx_mod;
        for (const char* n : dlls) {
            Support::ModuleHandle m = Support::LoadModule(n);
            if (m == nullptr) continue;
            pSave = (D3DXSaveSurfaceToFileAFn)GetProcAddress(m.get(), "D3DXSaveSurfaceToFileA");
            if (pSave != nullptr) {
                s_d3dx_mod = std::move(m);
                break;
            }
        }
        if (pSave == nullptr)
            LOG("D3D9", "D3DXSaveSurfaceToFileA not resolvable (screenshots disabled)");
    }
    if (pSave == nullptr) return false;

    constexpr DWORD D3DXIFF_PNG = 3;
    HRESULT const hr = pSave(path, D3DXIFF_PNG, backbuffer, nullptr, nullptr);
    if (FAILED(hr)) {
        LOG("D3D9", "SaveBackBufferToFile('%s') failed hr=0x%08lx", path, (unsigned long)hr);
        return false;
    }
    LOG("D3D9", "Screenshot saved: %s", path);
    return true;
}

bool D3D9State::ReadOffscreenBGRA(std::vector<uint8_t>& out, int& out_w, int& out_h) const {
    if ((device == nullptr) || (offscreen_rt == nullptr)) return false;

    D3DSURFACE_DESC desc{};
    if (FAILED(offscreen_rt->GetDesc(&desc))) return false;

    if ((g_readback_sysmem == nullptr) || std::cmp_not_equal(g_readback_w, desc.Width) ||
        std::cmp_not_equal(g_readback_h, desc.Height)) {
        if (g_readback_sysmem != nullptr) {
            g_readback_sysmem->Release();
            g_readback_sysmem = nullptr;
        }
        HRESULT const hr = device->CreateOffscreenPlainSurface(
            desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &g_readback_sysmem, nullptr);
        if (FAILED(hr) || (g_readback_sysmem == nullptr)) {
            LOG("D3D9",
                "ReadOffscreenBGRA: CreateOffscreenPlainSurface "
                "failed hr=0x%08lx",
                (unsigned long)hr);
            return false;
        }
        g_readback_w = (int)desc.Width;
        g_readback_h = (int)desc.Height;
    }

    HRESULT hr = device->GetRenderTargetData(offscreen_rt, g_readback_sysmem);
    if (FAILED(hr)) {
        LOG("D3D9", "ReadOffscreenBGRA: GetRenderTargetData failed hr=0x%08lx", (unsigned long)hr);
        return false;
    }

    D3DLOCKED_RECT lr{};
    hr = g_readback_sysmem->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    if (FAILED(hr)) {
        LOG("D3D9", "ReadOffscreenBGRA: LockRect failed hr=0x%08lx", (unsigned long)hr);
        return false;
    }

    const int w = (int)desc.Width;
    const int h = (int)desc.Height;
    const int row_bytes = w * 4;
    out.resize((size_t)row_bytes * h);
    const auto* src = reinterpret_cast<const uint8_t*>(lr.pBits);
    uint8_t* dst = out.data();
    for (int y = 0; y < h; y++) {
        memcpy(dst + ((ptrdiff_t)y * row_bytes), src + ((ptrdiff_t)y * lr.Pitch), row_bytes);
    }

    g_readback_sysmem->UnlockRect();
    out_w = w;
    out_h = h;
    return true;
}

void D3D9State::GetOffscreenSize(int& w, int& h) const {
    w = h = 0;
    if (offscreen_rt == nullptr) return;
    D3DSURFACE_DESC desc{};
    if (SUCCEEDED(offscreen_rt->GetDesc(&desc))) {
        w = (int)desc.Width;
        h = (int)desc.Height;
    }
}

void D3D9State::GetBackBufferSize(int& w, int& h) const {
    w = h = 0;
    if (backbuffer == nullptr) return;
    D3DSURFACE_DESC desc{};
    if (SUCCEEDED(backbuffer->GetDesc(&desc))) {
        w = (int)desc.Width;
        h = (int)desc.Height;
    }
}

namespace {

struct OverlayVertex {
    float x, y, z, rhw;
    DWORD color;
};

void AppendRect(std::vector<OverlayVertex>& out, float ax, float ay, float bx, float by,
                DWORD color) {
    out.push_back({.x = ax, .y = ay, .z = 0.0F, .rhw = 1.0F, .color = color});
    out.push_back({.x = bx, .y = ay, .z = 0.0F, .rhw = 1.0F, .color = color});
    out.push_back({.x = ax, .y = by, .z = 0.0F, .rhw = 1.0F, .color = color});
    out.push_back({.x = bx, .y = ay, .z = 0.0F, .rhw = 1.0F, .color = color});
    out.push_back({.x = bx, .y = by, .z = 0.0F, .rhw = 1.0F, .color = color});
    out.push_back({.x = ax, .y = by, .z = 0.0F, .rhw = 1.0F, .color = color});
}

void SetOverlayRenderStates(IDirect3DDevice9* device) {
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    device->SetTexture(0, nullptr);
    device->SetPixelShader(nullptr);
    device->SetVertexShader(nullptr);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE,
                           D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
                               D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
}

}

void D3D9State::DrawCropOverlay() const {
    if (device == nullptr) return;

    bool pick_mode = false;
    int rect_x = 0;
    int rect_y = 0;
    int rect_w = 0;
    int rect_h = 0;
    if (g_gpu.query_crop_overlay != nullptr)
        g_gpu.query_crop_overlay(&pick_mode, &rect_x, &rect_y, &rect_w, &rect_h);
    const bool has_rect = (rect_w > 0 && rect_h > 0);
    if (!has_rect && !pick_mode) return;

    int bb_w = 0;
    int bb_h = 0;
    GetBackBufferSize(bb_w, bb_h);
    int rt_w = 0;
    int rt_h = 0;
    GetOffscreenSize(rt_w, rt_h);
    if (bb_w <= 0 || bb_h <= 0 || rt_w <= 0 || rt_h <= 0) return;
    if (!has_rect) return;

    const float x0 = (float)rect_x * (float)bb_w / (float)rt_w;
    const float y0 = (float)rect_y * (float)bb_h / (float)rt_h;
    const float x1 = (float)(rect_x + rect_w) * (float)bb_w / (float)rt_w;
    const float y1 = (float)(rect_y + rect_h) * (float)bb_h / (float)rt_h;
    const auto bbw = (float)bb_w;
    const auto bbh = (float)bb_h;

    SetOverlayRenderStates(device);

    std::vector<OverlayVertex> dim_verts;
    dim_verts.reserve(24);
    const DWORD dim = 0xA0000000;
    AppendRect(dim_verts, 0.0F, 0.0F, bbw, y0, dim);
    AppendRect(dim_verts, 0.0F, y1, bbw, bbh, dim);
    AppendRect(dim_verts, 0.0F, y0, x0, y1, dim);
    AppendRect(dim_verts, x1, y0, bbw, y1, dim);
    device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, (UINT)(dim_verts.size() / 3), dim_verts.data(),
                            sizeof(OverlayVertex));

    const DWORD border = pick_mode ? 0xFFFFD21F : 0xFF22E0FF;
    const float t = 2.0F;
    std::vector<OverlayVertex> border_verts;
    border_verts.reserve(24);
    AppendRect(border_verts, x0 - t, y0 - t, x1 + t, y0, border);
    AppendRect(border_verts, x0 - t, y1, x1 + t, y1 + t, border);
    AppendRect(border_verts, x0 - t, y0, x0, y1, border);
    AppendRect(border_verts, x1, y0, x1 + t, y1, border);
    device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, (UINT)(border_verts.size() / 3),
                            border_verts.data(), sizeof(OverlayVertex));
}

bool D3D9State::SaveOffscreenRGBAToPNG(const char* path) const {
    if ((device == nullptr) || (offscreen_rt == nullptr)) return false;

    using D3DXSaveSurfaceToFileAFn =
        HRESULT(WINAPI*)(LPCSTR, DWORD, IDirect3DSurface9*, const PALETTEENTRY*, const RECT*);

    static D3DXSaveSurfaceToFileAFn pSave = nullptr;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        const char* dlls[] = {
            "d3dx9_43.dll", "d3dx9_42.dll", "d3dx9_41.dll", "d3dx9_40.dll", "d3dx9_39.dll",
            "d3dx9_38.dll", "d3dx9_37.dll", "d3dx9_36.dll", "d3dx9_35.dll", "d3dx9_34.dll",
            "d3dx9_33.dll", "d3dx9_32.dll", "d3dx9_31.dll", "d3dx9_30.dll", "d3dx9_29.dll",
            "d3dx9_28.dll", "d3dx9_27.dll", "d3dx9_26.dll", "d3dx9_25.dll", "d3dx9_24.dll",
        };
        static Support::ModuleHandle s_d3dx_mod;
        for (const char* n : dlls) {
            Support::ModuleHandle m = Support::LoadModule(n);
            if (m == nullptr) continue;
            pSave = (D3DXSaveSurfaceToFileAFn)GetProcAddress(m.get(), "D3DXSaveSurfaceToFileA");
            if (pSave != nullptr) {
                s_d3dx_mod = std::move(m);
                break;
            }
        }
        if (pSave == nullptr) {
            LOG("D3D9", "D3DXSaveSurfaceToFileA not resolvable "
                        "(offscreen PNG save disabled)");
        }
    }
    if (pSave == nullptr) return false;

    constexpr DWORD D3DXIFF_PNG = 3;
    HRESULT const hr = pSave(path, D3DXIFF_PNG, offscreen_rt, nullptr, nullptr);
    if (FAILED(hr)) {
        LOG("D3D9", "SaveOffscreenRGBAToPNG('%s') failed hr=0x%08lx", path, (unsigned long)hr);
        return false;
    }
    return true;
}
