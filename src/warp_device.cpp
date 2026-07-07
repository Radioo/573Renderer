#include "warp_device.h"

#include <d3d12.h>
#include <d3d9.h>
#include <d3d9on12.h>
#include <d3dcommon.h>
#include <dxgi.h>
#include <dxgi1_4.h>

#include <cstdio>
#include <string>

namespace WarpD3D9 {

namespace {

std::string g_last_error;

void Fail(const char* what, long hr) {
    char buf[160];
    snprintf(buf, sizeof(buf), "%s (hr=0x%08lx)", what, hr);
    g_last_error = buf;
}

HWND CreateHiddenWindow() {
    static bool registered = false;
    const char* kClass = "r573_warp_hidden";
    if (!registered) {
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = kClass;
        if (RegisterClassExA(&wc) == 0) {
            Fail("RegisterClassExA", (long)GetLastError());
            return nullptr;
        }
        registered = true;
    }
    HWND hwnd = CreateWindowExA(0, kClass, "r573 warp", WS_OVERLAPPED, 0, 0, 64, 64, nullptr,
                                nullptr, GetModuleHandleA(nullptr), nullptr);
    if (hwnd == nullptr) Fail("CreateWindowExA", (long)GetLastError());
    return hwnd;
}

IUnknown* CreateWarpD3D12Device() {
    HMODULE dxgi = LoadLibraryA("dxgi.dll");
    HMODULE d3d12 = LoadLibraryA("d3d12.dll");
    if (dxgi == nullptr || d3d12 == nullptr) {
        Fail("LoadLibrary dxgi/d3d12", 0);
        return nullptr;
    }
    using CreateFactoryFn = HRESULT(WINAPI*)(REFIID, void**);
    using CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
    auto create_factory = (CreateFactoryFn)GetProcAddress(dxgi, "CreateDXGIFactory1");
    auto create_device = (CreateDeviceFn)GetProcAddress(d3d12, "D3D12CreateDevice");
    if (create_factory == nullptr || create_device == nullptr) {
        Fail("GetProcAddress CreateDXGIFactory1/D3D12CreateDevice", 0);
        return nullptr;
    }

    IDXGIFactory4* factory = nullptr;
    HRESULT hr = create_factory(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        Fail("CreateDXGIFactory1(IDXGIFactory4)", hr);
        return nullptr;
    }
    IDXGIAdapter* warp = nullptr;
    hr = factory->EnumWarpAdapter(IID_PPV_ARGS(&warp));
    factory->Release();
    if (FAILED(hr)) {
        Fail("EnumWarpAdapter", hr);
        return nullptr;
    }
    ID3D12Device* dev12 = nullptr;
    hr = create_device(warp, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev12));
    warp->Release();
    if (FAILED(hr)) {
        Fail("D3D12CreateDevice(WARP)", hr);
        return nullptr;
    }
    return dev12;
}

IDirect3D9* Create9On12(IUnknown* dev12) {
    HMODULE d3d9 = LoadLibraryA("d3d9.dll");
    if (d3d9 == nullptr) {
        Fail("LoadLibrary d3d9", 0);
        return nullptr;
    }
    using Create9On12Fn = IDirect3D9*(WINAPI*)(UINT, D3D9ON12_ARGS*, UINT);
    auto create = (Create9On12Fn)GetProcAddress(d3d9, "Direct3DCreate9On12");
    if (create == nullptr) {
        Fail("Direct3DCreate9On12 export missing", 0);
        return nullptr;
    }
    D3D9ON12_ARGS args = {};
    args.Enable9On12 = TRUE;
    args.pD3D12Device = dev12;
    IDirect3D9* out = create(D3D_SDK_VERSION, &args, 1);
    if (out == nullptr) Fail("Direct3DCreate9On12", 0);
    return out;
}

IDirect3DDevice9* CreateD3D9Device(IDirect3D9* d3d9, HWND hwnd, int width, int height) {
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth = (UINT)width;
    pp.BackBufferHeight = (UINT)height;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.hDeviceWindow = hwnd;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    IDirect3DDevice9* device = nullptr;
    HRESULT const hr = d3d9->CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &pp, &device);
    if (FAILED(hr)) {
        Fail("IDirect3D9::CreateDevice(9on12/WARP)", hr);
        return nullptr;
    }
    return device;
}

}

Device::~Device() {
    if (device != nullptr) device->Release();
    if (d3d9 != nullptr) d3d9->Release();
    if (d3d12_device != nullptr) d3d12_device->Release();
    if (hwnd != nullptr) DestroyWindow(hwnd);
}

bool Create(Device& out, int width, int height) {
    out.hwnd = CreateHiddenWindow();
    if (out.hwnd == nullptr) return false;
    out.d3d12_device = CreateWarpD3D12Device();
    if (out.d3d12_device == nullptr) return false;
    out.d3d9 = Create9On12(out.d3d12_device);
    if (out.d3d9 == nullptr) return false;
    out.device = CreateD3D9Device(out.d3d9, out.hwnd, width, height);
    if (out.device == nullptr) return false;
    out.ok = true;
    return true;
}

const std::string& LastError() {
    return g_last_error;
}

}
