#include "gui/judge/judge_device.h"

#include "gui/gui_dpi.h"
#include "support/log.h"

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

#include <algorithm>
#include <cstring>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

namespace JudgeDevice {

namespace {

constexpr int kWindowW = 1700;
constexpr int kWindowH = 1060;
constexpr D3DCOLOR kWindowClear = D3DCOLOR_ARGB(255, 24, 24, 28);
constexpr std::uint8_t kOpaque = 255;

Window* g_window = nullptr;

LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp) != 0) return 1;
    switch (msg) {
    case WM_SIZE:
        if (g_window != nullptr && wp != SIZE_MINIMIZED) g_window->resize_pending = true;
        return 0;
    case WM_DPICHANGED: {
        Gui::Dpi::SetScaleFromDpi(HIWORD(wp));
        const auto* suggested = reinterpret_cast<const RECT*>(lp);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wp & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_CLOSE:
        if (g_window != nullptr) g_window->closed = true;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ClientSize(HWND hwnd, UINT& w, UINT& h) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    w = static_cast<UINT>(std::max<LONG>(rc.right - rc.left, 1));
    h = static_cast<UINT>(std::max<LONG>(rc.bottom - rc.top, 1));
}

bool CreateStages(Window& w) {
    for (StageTarget& stage : w.stages) {
        if (FAILED(w.device->CreateTexture(static_cast<UINT>(w.stage_w),
                                           static_cast<UINT>(w.stage_h), 1, D3DUSAGE_RENDERTARGET,
                                           D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &stage.texture,
                                           nullptr))) {
            return false;
        }
        stage.texture->GetSurfaceLevel(0, &stage.surface);
    }
    return SUCCEEDED(w.device->CreateDepthStencilSurface(
        static_cast<UINT>(w.stage_w), static_cast<UINT>(w.stage_h), D3DFMT_D24S8,
        D3DMULTISAMPLE_NONE, 0, TRUE, &w.depth, nullptr));
}

}

bool Create(Window& w, HINSTANCE instance, int stage_w, int stage_h, bool vsync) {
    Gui::Dpi::MakeThreadPerMonitorAware();
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"Renderer573Judge";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);
    w.hwnd = CreateWindowExW(0, wc.lpszClassName, L"573Renderer - Interpolation judge",
                             WS_OVERLAPPEDWINDOW, 40, 30, kWindowW, kWindowH, nullptr, nullptr,
                             instance, nullptr);
    if (w.hwnd == nullptr) return false;
    Gui::Dpi::SetScaleFromDpi(Gui::Dpi::DpiForWindow(w.hwnd));
    g_window = &w;

    if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &w.d3d))) return false;
    w.pp = {};
    w.pp.Windowed = TRUE;
    w.pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    w.pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    ClientSize(w.hwnd, w.pp.BackBufferWidth, w.pp.BackBufferHeight);
    w.pp.PresentationInterval = vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
    w.pp.hDeviceWindow = w.hwnd;
    HRESULT hr =
        w.d3d->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, w.hwnd,
                              D3DCREATE_HARDWARE_VERTEXPROCESSING, &w.pp, nullptr, &w.device);
    if (FAILED(hr)) {
        hr = w.d3d->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, w.hwnd,
                                   D3DCREATE_SOFTWARE_VERTEXPROCESSING, &w.pp, nullptr, &w.device);
    }
    if (FAILED(hr)) {
        LOG("Judge", "CreateDeviceEx failed (hr=0x%08lx)", static_cast<unsigned long>(hr));
        return false;
    }
    w.stage_w = stage_w;
    w.stage_h = stage_h;
    if (!CreateStages(w)) return false;
    ShowWindow(w.hwnd, SW_SHOW);
    UpdateWindow(w.hwnd);
    return true;
}

void Destroy(Window& w) {
    for (StageTarget& stage : w.stages) {
        if (stage.surface != nullptr) stage.surface->Release();
        if (stage.texture != nullptr) stage.texture->Release();
        stage = {};
    }
    if (w.depth != nullptr) w.depth->Release();
    if (w.device != nullptr) w.device->Release();
    if (w.d3d != nullptr) w.d3d->Release();
    if (w.hwnd != nullptr) DestroyWindow(w.hwnd);
    w = {};
    g_window = nullptr;
}

bool Pump(Window& w) {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) w.closed = true;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return !w.closed;
}

void ApplyResize(Window& w) {
    if (!w.resize_pending) return;
    w.resize_pending = false;
    ClientSize(w.hwnd, w.pp.BackBufferWidth, w.pp.BackBufferHeight);
    ImGui_ImplDX9_InvalidateDeviceObjects();
    const HRESULT hr = w.device->ResetEx(&w.pp, nullptr);
    if (FAILED(hr)) LOG("Judge", "ResetEx failed (hr=0x%08lx)", static_cast<unsigned long>(hr));
    ImGui_ImplDX9_CreateDeviceObjects();
}

void BeginStage(Window& w, std::size_t index, const Judge::BackdropFill& backdrop) {
    w.device->SetRenderTarget(0, w.stages.at(index).surface);
    w.device->SetDepthStencilSurface(w.depth);
    w.device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                    backdrop.base, 1.0F, 0);
    if (backdrop.rects.empty()) return;
    std::vector<D3DRECT> rects;
    rects.reserve(backdrop.rects.size());
    for (const Judge::BackdropRect& r : backdrop.rects)
        rects.push_back({r.x0, r.y0, r.x1, r.y1});
    w.device->Clear(static_cast<DWORD>(rects.size()), rects.data(), D3DCLEAR_TARGET,
                    backdrop.squares, 1.0F, 0);
}

IDirect3DSurface9* BackBuffer(Window& w) {
    IDirect3DSurface9* back = nullptr;
    w.device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back);
    return back;
}

void BeginWindow(Window& w) {
    IDirect3DSurface9* back = BackBuffer(w);
    w.device->SetRenderTarget(0, back);
    if (back != nullptr) back->Release();
    w.device->SetDepthStencilSurface(nullptr);
    D3DVIEWPORT9 const vp{0, 0, w.pp.BackBufferWidth, w.pp.BackBufferHeight, 0.0F, 1.0F};
    w.device->SetViewport(&vp);
    w.device->Clear(0, nullptr, D3DCLEAR_TARGET, kWindowClear, 1.0F, 0);
}

void Present(Window& w) {
    w.device->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
}

bool ReadSurface(Window& w, IDirect3DSurface9* surface, std::vector<std::uint8_t>& bgra, int& width,
                 int& height) {
    D3DSURFACE_DESC desc{};
    if (surface == nullptr || FAILED(surface->GetDesc(&desc))) return false;
    IDirect3DSurface9* copy = nullptr;
    if (FAILED(w.device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
                                                     D3DPOOL_SYSTEMMEM, &copy, nullptr))) {
        return false;
    }
    bool ok = SUCCEEDED(w.device->GetRenderTargetData(surface, copy));
    D3DLOCKED_RECT locked{};
    if (ok) ok = SUCCEEDED(copy->LockRect(&locked, nullptr, D3DLOCK_READONLY));
    if (ok) {
        width = static_cast<int>(desc.Width);
        height = static_cast<int>(desc.Height);
        bgra.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
        for (int y = 0; y < height; y++) {
            const auto* row =
                static_cast<const std::uint8_t*>(locked.pBits) +
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(locked.Pitch));
            std::memcpy(bgra.data() +
                            (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) * 4),
                        row, static_cast<std::size_t>(width) * 4);
        }
        copy->UnlockRect();
        if (desc.Format == D3DFMT_X8R8G8B8) {
            for (std::size_t i = 3; i < bgra.size(); i += 4)
                bgra[i] = kOpaque;
        }
    }
    copy->Release();
    return ok;
}

}
