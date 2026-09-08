#include "gui_dpi.h"

#include "../support/log.h"

#include <windows.h>

#include <algorithm>

namespace Gui::Dpi {

namespace {

constexpr unsigned kReferenceDpi = 96;
constexpr float kScaleMin = 1.0F;
constexpr float kScaleMax = 4.0F;

float g_scale = 1.0F;

using SetThreadDpiAwarenessContextFn = DPI_AWARENESS_CONTEXT(WINAPI*)(DPI_AWARENESS_CONTEXT);
using GetDpiForWindowFn = UINT(WINAPI*)(HWND);

template <typename Fn> Fn User32Entry(const char* name) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32 == nullptr) return nullptr;
    return reinterpret_cast<Fn>(GetProcAddress(user32, name));
}

unsigned DesktopDpi() {
    HDC dc = GetDC(nullptr);
    if (dc == nullptr) return kReferenceDpi;
    int const dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return dpi > 0 ? (unsigned)dpi : kReferenceDpi;
}

}

void MakeThreadPerMonitorAware() {
    auto const set_context =
        User32Entry<SetThreadDpiAwarenessContextFn>("SetThreadDpiAwarenessContext");
    if (set_context == nullptr) {
        LOG("GuiDpi", "SetThreadDpiAwarenessContext unavailable - staying system DPI aware");
        return;
    }
    if (set_context(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) == nullptr) {
        LOG("GuiDpi", "SetThreadDpiAwarenessContext failed (err=%lu)", GetLastError());
    }
}

unsigned DpiForWindow(HWND hwnd) {
    auto const get_dpi = User32Entry<GetDpiForWindowFn>("GetDpiForWindow");
    if (get_dpi != nullptr && hwnd != nullptr) {
        UINT const dpi = get_dpi(hwnd);
        if (dpi > 0) return dpi;
    }
    return DesktopDpi();
}

void SetScaleFromDpi(unsigned dpi) {
    float const raw = (float)dpi / (float)kReferenceDpi;
    g_scale = std::clamp(raw, kScaleMin, kScaleMax);
}

float Scale() {
    return g_scale;
}

}
