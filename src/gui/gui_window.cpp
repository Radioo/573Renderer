#include "gui_window.h"
#include "gui_panels.h"
#include "gui_layout_constants.h"
#include "../support/log.h"

#include <algorithm>
#include <d3d9.h>
#include <d3d9caps.h>
#include <string>

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
        RECT r = {0, 0, kMinClientW, kMinClientH};
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        mmi->ptMinTrackSize.x = r.right - r.left;
        mmi->ptMinTrackSize.y = r.bottom - r.top;
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wp & 0xfff0) == SC_KEYMENU) return 0;
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
        return false;
    }
    return true;
}
}

namespace {
void LoadFonts() {
    static const ImWchar kGlyphRanges[] = {
        0x0020, 0x00FF, 0x2010, 0x2027, 0x2030, 0x205E, 0,
    };

    ImGuiIO& io = ImGui::GetIO();

    char winroot[MAX_PATH] = {};
    UINT const n = GetWindowsDirectoryA(winroot, sizeof(winroot));
    if (n == 0 || n >= sizeof(winroot)) {
        LOG("Gui", "GetWindowsDirectory failed; using default ImGui font");
        io.Fonts->AddFontDefault();
        return;
    }

    struct Candidate {
        const char* rel_path;
        float size_px;
        const char* label;
    };
    const Candidate candidates[] = {
        {.rel_path = "\\Fonts\\segoeui.ttf", .size_px = 16.0F, .label = "Segoe UI 16px"},
        {.rel_path = "\\Fonts\\consola.ttf", .size_px = 15.0F, .label = "Consolas 15px"},
    };

    for (const auto& c : candidates) {
        std::string const path = std::string(winroot) + c.rel_path;
        ImFont* f = io.Fonts->AddFontFromFileTTF(path.c_str(), c.size_px, nullptr, kGlyphRanges);
        if (f != nullptr) {
            LOG("Gui", "Font loaded: %s (%s)", c.label, path.c_str());
            return;
        }
    }

    LOG("Gui", "Could not load Segoe UI or Consolas; using ImGui default "
               "(em-dashes will render as boxes)");
    io.Fonts->AddFontDefault();
}
}

namespace {
void ApplyDarkStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 6.0F;
    s.FrameRounding = 4.0F;
    s.GrabRounding = 4.0F;
    s.ScrollbarRounding = 4.0F;
    s.TabRounding = 4.0F;
    s.PopupRounding = 4.0F;
    s.ChildRounding = 4.0F;
    s.WindowBorderSize = 0.0F;
    s.FrameBorderSize = 0.0F;
    s.ItemSpacing = ImVec2(8, 6);
    s.ItemInnerSpacing = ImVec2(6, 4);
    s.FramePadding = ImVec2(8, 4);
    s.WindowPadding = ImVec2(12, 10);

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.10F, 0.105F, 0.11F, 1.00F);
    c[ImGuiCol_ChildBg] = ImVec4(0.12F, 0.125F, 0.13F, 1.00F);
    c[ImGuiCol_PopupBg] = ImVec4(0.08F, 0.08F, 0.09F, 0.98F);
    c[ImGuiCol_Border] = ImVec4(0.18F, 0.18F, 0.20F, 0.50F);
    c[ImGuiCol_FrameBg] = ImVec4(0.16F, 0.16F, 0.18F, 1.00F);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.22F, 0.22F, 0.25F, 1.00F);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.26F, 0.26F, 0.30F, 1.00F);
    c[ImGuiCol_TitleBg] = ImVec4(0.08F, 0.08F, 0.09F, 1.00F);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.10F, 0.10F, 0.12F, 1.00F);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.06F, 0.06F, 0.07F, 1.00F);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.11F, 0.11F, 0.13F, 1.00F);
    c[ImGuiCol_Button] = ImVec4(0.22F, 0.34F, 0.55F, 1.00F);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.30F, 0.44F, 0.70F, 1.00F);
    c[ImGuiCol_ButtonActive] = ImVec4(0.20F, 0.32F, 0.52F, 1.00F);
    c[ImGuiCol_Header] = ImVec4(0.18F, 0.22F, 0.30F, 1.00F);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.24F, 0.30F, 0.40F, 1.00F);
    c[ImGuiCol_HeaderActive] = ImVec4(0.20F, 0.26F, 0.35F, 1.00F);
    c[ImGuiCol_CheckMark] = ImVec4(0.55F, 0.82F, 1.00F, 1.00F);
    c[ImGuiCol_SliderGrab] = ImVec4(0.45F, 0.65F, 0.90F, 1.00F);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.55F, 0.75F, 1.00F, 1.00F);
    c[ImGuiCol_Tab] = ImVec4(0.14F, 0.14F, 0.16F, 1.00F);
    c[ImGuiCol_TabHovered] = ImVec4(0.28F, 0.38F, 0.55F, 1.00F);
    c[ImGuiCol_TabActive] = ImVec4(0.22F, 0.30F, 0.45F, 1.00F);
    c[ImGuiCol_Separator] = ImVec4(0.22F, 0.22F, 0.25F, 1.00F);
    c[ImGuiCol_Text] = ImVec4(0.92F, 0.92F, 0.93F, 1.00F);
    c[ImGuiCol_TextDisabled] = ImVec4(0.45F, 0.45F, 0.48F, 1.00F);
}
}

bool Init(Window& w, HINSTANCE hinst) {
    w.wc.cbSize = sizeof(w.wc);
    w.wc.style = CS_CLASSDC;
    w.wc.lpfnWndProc = WndProc;
    w.wc.hInstance = hinst;
    w.wc.lpszClassName = L"Renderer573Gui";
    w.wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&w.wc);

    w.hwnd = CreateWindowExW(0, w.wc.lpszClassName, L"573Renderer - Control", WS_OVERLAPPEDWINDOW,
                             80, 60, 1360, 820, nullptr, nullptr, hinst, nullptr);
    if (w.hwnd == nullptr) {
        UnregisterClassW(w.wc.lpszClassName, hinst);
        return false;
    }

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

    ApplyDarkStyle();

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
    return true;
}

}
