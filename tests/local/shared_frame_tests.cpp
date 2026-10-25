#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "render_backend.h"
#include "shared_frame.h"
#include "support/com_ptr.h"

#include <windows.h>

#include <d3d9.h>

#include <cstdint>
#include <string>

namespace {

constexpr int kWidth = 64;
constexpr int kHeight = 48;
constexpr D3DCOLOR kFill = D3DCOLOR_ARGB(255, 10, 200, 30);

HWND HiddenWindow() {
    const char* kClass = "r573_shared_frame_test";
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = kClass;
    RegisterClassExA(&wc);
    return CreateWindowExA(0, kClass, "shared frame", WS_OVERLAPPED, 0, 0, kWidth, kHeight, nullptr,
                           nullptr, GetModuleHandleA(nullptr), nullptr);
}

struct Reader {
    ComPtr<IDirect3D9Ex> d3d;
    ComPtr<IDirect3DDevice9Ex> device;
};

bool OpenReader(Reader& reader, HWND hwnd) {
    if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &reader.d3d))) return false;
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.hDeviceWindow = hwnd;
    return SUCCEEDED(reader.d3d->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                                D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, nullptr,
                                                &reader.device));
}

bool ReadFirstPixel(Reader& reader, HANDLE handle, int width, int height, uint32_t& pixel) {
    ComPtr<IDirect3DTexture9> opened;
    HANDLE shared = handle;
    if (FAILED(reader.device->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET,
                                            D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &opened, &shared))) {
        return false;
    }
    ComPtr<IDirect3DSurface9> level;
    ComPtr<IDirect3DSurface9> copy;
    if (FAILED(opened->GetSurfaceLevel(0, &level))) return false;
    if (FAILED(reader.device->CreateOffscreenPlainSurface(width, height, D3DFMT_A8R8G8B8,
                                                          D3DPOOL_SYSTEMMEM, &copy, nullptr))) {
        return false;
    }
    if (FAILED(reader.device->GetRenderTargetData(level, copy))) return false;
    D3DLOCKED_RECT locked = {};
    if (FAILED(copy->LockRect(&locked, nullptr, D3DLOCK_READONLY))) return false;
    pixel = *static_cast<const uint32_t*>(locked.pBits);
    copy->UnlockRect();
    return true;
}

}

TEST_CASE("A rendered frame reaches another device through the shared texture") {
    HWND hwnd = HiddenWindow();
    REQUIRE(hwnd != nullptr);
    D3D9State host;
    host.width = kWidth;
    host.height = kHeight;
    REQUIRE(host.Init(hwnd));
    REQUIRE(SUCCEEDED(host.device->ColorFill(host.offscreen_rt, nullptr, kFill)));

    auto target = SharedFrame::Create(host.device, kWidth, kHeight);
    REQUIRE(target.has_value());
    CHECK(target->handle != nullptr);
    const auto copied = SharedFrame::Copy(host.device, host.offscreen_rt, *target);
    INFO((copied ? std::string() : copied.error()));
    REQUIRE(copied.has_value());

    Reader reader;
    REQUIRE(OpenReader(reader, hwnd));
    uint32_t pixel = 0;
    REQUIRE(ReadFirstPixel(reader, target->handle, kWidth, kHeight, pixel));
    CHECK(pixel == kFill);

    auto resized = SharedFrame::Create(host.device, kWidth * 2, kHeight * 2);
    REQUIRE(resized.has_value());
    CHECK(resized->handle != target->handle);
    CHECK(resized->width == kWidth * 2);

    host.Shutdown();
    DestroyWindow(hwnd);
}
