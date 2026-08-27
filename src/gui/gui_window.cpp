#include "gui_window.h"

#include "../state/app_state.h"
#include "../warp_device.h"
#include "gui_dpi.h"
#include "gui_panels.h"
#include "gui_layout_constants.h"
#include "gui_style.h"
#include "../support/log.h"

#include <algorithm>
#include <d3d9.h>
#include <d3d9caps.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx9.h>

#pragma comment(lib, "d3d9.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

namespace Gui {

namespace {
HWND g_current_hwnd = nullptr;
}

HWND GetHwnd() {
    return g_current_hwnd;
}

namespace {
Window* g_win = nullptr;
}

IDirect3DDevice9* GetDevice() {
    return (g_win != nullptr) ? g_win->device : nullptr;
}

namespace {
bool ResetDevice(Window& w);
}
namespace {
void RenderFrameLocked(Window& w);
}

namespace {
LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp) != 0) return 1;
    switch (msg) {
    case WM_SIZE:
        if ((g_win != nullptr) && wp != SIZE_MINIMIZED) {
            g_win->resize_pending = true;
            RenderFrameLocked(*g_win);
        }
        return 0;
    case WM_PAINT:
        if (g_win != nullptr) RenderFrameLocked(*g_win);
        ValidateRect(hwnd, nullptr);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_GETMINMAXINFO: {
        RECT r = {0, 0, (LONG)Dpi::S(kMinClientW), (LONG)Dpi::S(kMinClientH)};
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        mmi->ptMinTrackSize.x = r.right - r.left;
        mmi->ptMinTrackSize.y = r.bottom - r.top;
        return 0;
    }
    case WM_DPICHANGED: {
        Dpi::SetScaleFromDpi(HIWORD(wp));
        ApplyStyle();
        const auto* suggested = reinterpret_cast<const RECT*>(lp);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        LOG("Gui", "GUI window DPI changed to %u (scale %.2f)", (unsigned)HIWORD(wp), Dpi::Scale());
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wp & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_CLOSE:
        if (App::Global().CloseNeedsPrompt() && !App::Global().CloseRequested()) {
            App::Global().PostCloseRequest();
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
}

namespace {
bool CreateDevice(Window& w) {
    w.d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (w.d3d == nullptr) return false;

    w.pp = {};
    w.pp.Windowed = TRUE;
    w.pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    w.pp.BackBufferFormat = D3DFMT_UNKNOWN;
    w.pp.EnableAutoDepthStencil = TRUE;
    w.pp.AutoDepthStencilFormat = D3DFMT_D16;
    w.pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    w.pp.hDeviceWindow = w.hwnd;

    HRESULT hr = w.d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, w.hwnd,
                                     D3DCREATE_HARDWARE_VERTEXPROCESSING, &w.pp, &w.device);
    if (FAILED(hr)) {
        hr = w.d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, w.hwnd,
                                 D3DCREATE_SOFTWARE_VERTEXPROCESSING, &w.pp, &w.device);
    }
    if (FAILED(hr) || (w.device == nullptr)) {
        w.d3d->Release();
        w.d3d = nullptr;
        w.device = nullptr;
        RECT rc{};
        GetClientRect(w.hwnd, &rc);
        int const cw = std::max<int>(rc.right - rc.left, 1);
        int const ch = std::max<int>(rc.bottom - rc.top, 1);
        if (!WarpD3D9::CreateForWindow(w.warp, w.hwnd, cw, ch)) {
            LOG("Gui", "D3D9 HAL and WARP both unavailable: %s", WarpD3D9::LastError().c_str());
            return false;
        }
        w.warp_backed = true;
        w.device = w.warp.device;
        w.pp.BackBufferWidth = (UINT)cw;
        w.pp.BackBufferHeight = (UINT)ch;
        w.pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        w.pp.EnableAutoDepthStencil = FALSE;
        LOG("Gui", "D3D9 HAL unavailable, GUI running on the WARP 9on12 device");
    }
    return true;
}
}

namespace {
RECT WorkAreaOf(HWND hwnd) {
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if ((mon != nullptr) && GetMonitorInfoW(mon, &mi) != 0) return mi.rcWork;
    RECT rc = {0, 0, 0, 0};
    GetWindowRect(hwnd, &rc);
    return rc;
}

void SizeToDpi(HWND hwnd) {
    Dpi::SetScaleFromDpi(Dpi::DpiForWindow(hwnd));

    const RECT work = WorkAreaOf(hwnd);
    const LONG work_w = std::max<LONG>(work.right - work.left, 1);
    const LONG work_h = std::max<LONG>(work.bottom - work.top, 1);
    const LONG wanted_w = std::min<LONG>((LONG)Dpi::S(kDefaultWindowW), work_w);
    const LONG wanted_h = std::min<LONG>((LONG)Dpi::S(kDefaultWindowH), work_h);

    RECT current = {0, 0, 0, 0};
    GetWindowRect(hwnd, &current);
    const LONG x = std::clamp(current.left, work.left, work.right - wanted_w);
    const LONG y = std::clamp(current.top, work.top, work.bottom - wanted_h);

    SetWindowPos(hwnd, nullptr, x, y, wanted_w, wanted_h, SWP_NOZORDER | SWP_NOACTIVATE);
    LOG("Gui", "GUI window DPI %u (scale %.2f), placed %ldx%ld at %ld,%ld", Dpi::DpiForWindow(hwnd),
        Dpi::Scale(), wanted_w, wanted_h, x, y);
}
}

bool Init(Window& w, HINSTANCE hinst) {
    Dpi::MakeThreadPerMonitorAware();

    w.wc.cbSize = sizeof(w.wc);
    w.wc.style = CS_CLASSDC;
    w.wc.lpfnWndProc = WndProc;
    w.wc.hInstance = hinst;
    w.wc.lpszClassName = L"Renderer573Gui";
    w.wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&w.wc);

    w.hwnd =
        CreateWindowExW(0, w.wc.lpszClassName, L"573Renderer - Control", WS_OVERLAPPEDWINDOW, 80,
                        60, kDefaultWindowW, kDefaultWindowH, nullptr, nullptr, hinst, nullptr);
    if (w.hwnd == nullptr) {
        UnregisterClassW(w.wc.lpszClassName, hinst);
        return false;
    }

    SizeToDpi(w.hwnd);

    if (!CreateDevice(w)) {
        DestroyWindow(w.hwnd);
        UnregisterClassW(w.wc.lpszClassName, hinst);
        LOG("Gui", "Failed to create D3D9 device for GUI window");
        return false;
    }

    ShowWindow(w.hwnd, SW_SHOW);
    UpdateWindow(w.hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    LoadFonts();

    ApplyStyle();

    ImGui_ImplWin32_Init(w.hwnd);
    ImGui_ImplDX9_Init(w.device);

    g_current_hwnd = w.hwnd;
    g_win = &w;
    LOG("Gui", "GUI window initialised (HWND=%p, device=%p)", w.hwnd, w.device);
    return true;
}

void Shutdown(Window& w) {
    g_win = nullptr;
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    if (w.warp_backed) {
        w.device = nullptr;
        w.warp_backed = false;
    }
    if (w.device != nullptr) {
        w.device->Release();
        w.device = nullptr;
    }
    if (w.d3d != nullptr) {
        w.d3d->Release();
        w.d3d = nullptr;
    }
    if (w.hwnd != nullptr) {
        DestroyWindow(w.hwnd);
        w.hwnd = nullptr;
    }
    UnregisterClassW(w.wc.lpszClassName, w.wc.hInstance);
    g_current_hwnd = nullptr;
}

namespace {
bool ResetDevice(Window& w) {
    RECT rc{};
    GetClientRect(w.hwnd, &rc);
    LONG cw = rc.right - rc.left;
    LONG ch = rc.bottom - rc.top;
    cw = std::max<LONG>(cw, 1);
    ch = std::max<LONG>(ch, 1);
    w.pp.BackBufferWidth = (UINT)cw;
    w.pp.BackBufferHeight = (UINT)ch;

    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT const hr = w.device->Reset(&w.pp);
    if (hr == D3DERR_DEVICELOST) {
        w.device_lost = true;
        return false;
    }
    if (FAILED(hr)) LOG("Gui", "device Reset failed (hr=0x%08lx)", hr);
    ImGui_ImplDX9_CreateDeviceObjects();
    return true;
}
}

namespace {
void RenderFrameLocked(Window& w) {
    static bool in_frame = false;
    if (in_frame || (w.device == nullptr)) return;
    in_frame = true;

    if (w.resize_pending) {
        w.resize_pending = false;
        if (!w.device_lost) ResetDevice(w);
    }

    if (w.device_lost) {
        HRESULT const hr = w.device->TestCooperativeLevel();
        if (hr == D3DERR_DEVICENOTRESET) {
            if (!ResetDevice(w)) {
                in_frame = false;
                return;
            }
            w.device_lost = false;
        } else if (hr == D3DERR_DEVICELOST) {
            in_frame = false;
            return;
        } else {
            w.device_lost = false;
        }
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    Panels::Build();

    ImGui::EndFrame();

    w.device->SetRenderState(D3DRS_ZENABLE, FALSE);
    w.device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    w.device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    w.device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(16, 17, 20, 255),
                    1.0F, 0);

    if (SUCCEEDED(w.device->BeginScene())) {
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        w.device->EndScene();
    }

    HRESULT const hr = w.device->Present(nullptr, nullptr, nullptr, nullptr);
    if (hr == D3DERR_DEVICELOST) w.device_lost = true;

    in_frame = false;
}
}

bool PumpAndRender(Window& w) {
    if (w.hwnd == nullptr) return false;

    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_QUIT) return false;
    }

    RenderFrameLocked(w);
    return !App::Global().CloseConfirmed();
}

}
