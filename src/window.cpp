#include "window.h"
#include "state/app_state.h"
#include "state/live_controls.h"
#include "support/log.h"

#include <cstdint>
#include <windowsx.h>

#include <algorithm>

namespace {
bool g_running = true;
}

namespace {
int g_rt_w = 1920;
}
namespace {
int g_rt_h = 1080;
}

namespace {
bool g_crop_drag_active = false;
}
namespace {
POINT g_crop_drag_anchor = {0, 0};
}

namespace {
void ClientToRt(HWND hwnd, POINT p, int& rt_x, int& rt_y) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int cw = (std::max)(1L, rc.right - rc.left);
    const int ch = (std::max)(1L, rc.bottom - rc.top);
    const int px = std::clamp((int)p.x, 0, cw);
    const int py = std::clamp((int)p.y, 0, ch);
    rt_x = (int)((int64_t)px * g_rt_w / cw);
    rt_y = (int)((int64_t)py * g_rt_h / ch);
}
}

namespace {
void PublishDragRect(HWND hwnd, POINT current) {
    int ax = 0;
    int ay = 0;
    int cx = 0;
    int cy = 0;
    ClientToRt(hwnd, g_crop_drag_anchor, ax, ay);
    ClientToRt(hwnd, current, cx, cy);
    int const x0 = (std::min)(ax, cx);
    int const y0 = (std::min)(ay, cy);
    int const x1 = (std::max)(ax, cx);
    int const y1 = (std::max)(ay, cy);
    App::CropRect r;
    r.x = x0;
    r.y = y0;
    r.w = x1 - x0;
    r.h = y1 - y0;
    App::Global().SetCropRect(r);
}
}

namespace {
bool HandleCropPick(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, App::State& state,
                    LRESULT& result) {
    result = 0;
    switch (msg) {
    case WM_KEYDOWN:
        if (wParam != VK_ESCAPE) return false;
        if (g_crop_drag_active) {
            ReleaseCapture();
            g_crop_drag_active = false;
        }
        state.SetCropPickMode(false);
        return true;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(LoadCursor(nullptr, IDC_CROSS));
            result = TRUE;
            return true;
        }
        return false;
    case WM_LBUTTONDOWN: {
        SetCapture(hwnd);
        g_crop_drag_active = true;
        g_crop_drag_anchor.x = GET_X_LPARAM(lParam);
        g_crop_drag_anchor.y = GET_Y_LPARAM(lParam);
        POINT const p = g_crop_drag_anchor;
        PublishDragRect(hwnd, p);
        return true;
    }
    case WM_MOUSEMOVE:
        if (g_crop_drag_active) {
            POINT const p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            PublishDragRect(hwnd, p);
            return true;
        }
        return false;
    case WM_LBUTTONUP:
        if (g_crop_drag_active) {
            POINT const p{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            PublishDragRect(hwnd, p);
            ReleaseCapture();
            g_crop_drag_active = false;
            state.SetCropPickMode(false);
            App::CropRect const r = state.GetCropRect();
            if (r.w <= 0 || r.h <= 0) state.SetCropRect({});
            return true;
        }
        return false;
    default:
        return false;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto& state = App::Global();
    if (state.GetCropPickMode()) {
        LRESULT result = 0;
        if (HandleCropPick(hwnd, msg, wParam, lParam, state, result)) return result;
    }

    switch (msg) {
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            g_running = false;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_CAPTURECHANGED:
        if (g_crop_drag_active) {
            g_crop_drag_active = false;
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}
}

HWND AppWindow::Create(int w, int h) {
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "AFPRendererClass";
    RegisterClassExA(&wc);

    RECT rc = {0, 0, w, h};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    const int outer_w = rc.right - rc.left;
    const int outer_h = rc.bottom - rc.top;

    HWND hwnd =
        CreateWindowExA(0, "AFPRendererClass", "AFP IFS Renderer", WS_OVERLAPPEDWINDOW, 0, 0,
                        outer_w, outer_h, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);

    if (hwnd != nullptr) {
        SetWindowPos(hwnd, nullptr, 0, 0, outer_w, outer_h,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);

        RECT client_rc{};
        GetClientRect(hwnd, &client_rc);
        const int got_w = client_rc.right - client_rc.left;
        const int got_h = client_rc.bottom - client_rc.top;
        if (got_w != w || got_h != h) {
            LOG("Window",
                "WARNING: requested client %dx%d but got %dx%d "
                "after SetWindowPos - Present will scale, expect distortion",
                w, h, got_w, got_h);
        } else {
            LOG("Window", "Client area: %dx%d (1:1 with backbuffer)", got_w, got_h);
        }

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    }
    return hwnd;
}

void AppWindow::Resize(HWND hwnd, int w, int h) {
    if (hwnd == nullptr) return;
    RECT rc = {0, 0, w, h};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    SetWindowPos(hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOMOVE | SWP_NOZORDER);
}

bool AppWindow::PumpMessages() {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE) != 0) {
        if (msg.message == WM_QUIT) {
            g_running = false;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return g_running;
}

bool AppWindow::IsRunning() {
    return g_running;
}
void AppWindow::RequestClose() {
    g_running = false;
}

void AppWindow::SetRenderRtSize(int w, int h) {
    if (w > 0 && h > 0) {
        g_rt_w = w;
        g_rt_h = h;
    }
}
