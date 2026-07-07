#pragma once
#include <windows.h>

#include <d3d9.h>

#include <string>

namespace WarpD3D9 {

struct Device {
    HWND hwnd = nullptr;
    IDirect3D9* d3d9 = nullptr;
    IDirect3DDevice9* device = nullptr;
    IUnknown* d3d12_device = nullptr;
    bool ok = false;

    Device() = default;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;
    ~Device();
};

bool Create(Device& out, int width, int height);

const std::string& LastError();

}
