#pragma once

#include <windows.h>
#include <d3d9.h>

namespace Gui {

struct Window {
    HWND hwnd = nullptr;
    WNDCLASSEXW wc = {};
    IDirect3D9* d3d = nullptr;
    IDirect3DDevice9* device = nullptr;
    D3DPRESENT_PARAMETERS pp = {};
    bool device_lost = false;
    bool resize_pending = false;
};

bool Init(Window& w, HINSTANCE hinst);

void Shutdown(Window& w);

bool PumpAndRender(Window& w);

HWND GetHwnd();

}
