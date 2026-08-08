#include "scene3d/scene3d_input.h"

#include "scene3d/camera.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Scene3d {

namespace {

bool g_enabled = false;
bool g_looking = false;
POINT g_anchor = {0, 0};
float g_dx = 0.0F;
float g_dy = 0.0F;

void BeginLook(HWND hwnd) {
    g_looking = true;
    SetCapture(hwnd);
    GetCursorPos(&g_anchor);
    ShowCursor(FALSE);
}

void EndLook() {
    if (!g_looking) return;
    g_looking = false;
    ReleaseCapture();
    ShowCursor(TRUE);
}

bool Down(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

}

void SetInputEnabled(bool enabled) {
    g_enabled = enabled;
    if (!enabled) EndLook();
}

bool LookActive() {
    return g_looking;
}

bool HandleLookMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    (void)wparam;
    (void)lparam;
    if (!g_enabled) return false;

    switch (msg) {
    case WM_RBUTTONDOWN:
        BeginLook(hwnd);
        return true;
    case WM_RBUTTONUP:
        EndLook();
        return true;
    case WM_CAPTURECHANGED:
        if (g_looking) {
            g_looking = false;
            ShowCursor(TRUE);
        }
        return false;
    case WM_MOUSEMOVE: {
        if (!g_looking) return false;
        POINT p;
        GetCursorPos(&p);
        g_dx += (float)(p.x - g_anchor.x);
        g_dy += (float)(p.y - g_anchor.y);
        SetCursorPos(g_anchor.x, g_anchor.y);
        return true;
    }
    default:
        return false;
    }
}

void PollCameraInput(CameraInput& out) {
    out = CameraInput{};
    out.look_dx = g_dx;
    out.look_dy = g_dy;
    g_dx = 0.0F;
    g_dy = 0.0F;
    if (!g_enabled || !g_looking) return;

    out.forward = Down('W');
    out.back = Down('S');
    out.left = Down('A');
    out.right = Down('D');
    out.up = Down('E') || Down(VK_SPACE);
    out.down = Down('Q') || Down(VK_CONTROL);
    out.fast = Down(VK_SHIFT);
    out.slow = Down(VK_MENU);
}

}
