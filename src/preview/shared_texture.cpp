#include "preview/shared_texture.h"

#include "support/com_ptr.h"
#include "support/expected.h"

#include <windows.h>

#include <d3d9.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <string>
#include <vector>

namespace SharedTexture {

namespace {

constexpr std::size_t kBytesPerPixel = 4;

}

Support::Expected<Reader, std::string> Reader::Create() {
    Reader reader;
    if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &reader.d3d_)))
        return Support::Unexpected(std::string("Direct3DCreate9Ex failed"));
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.hDeviceWindow = GetDesktopWindow();
    const HRESULT created = reader.d3d_->CreateDeviceEx(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, pp.hDeviceWindow, D3DCREATE_HARDWARE_VERTEXPROCESSING,
        &pp, nullptr, &reader.device_);
    if (FAILED(created)) {
        return Support::Unexpected(
            std::format("CreateDeviceEx failed ({:#x})", static_cast<uint32_t>(created)));
    }
    return reader;
}

Support::Expected<std::vector<uint8_t>, std::string> Reader::Read(uint64_t handle, uint32_t width,
                                                                  uint32_t height) {
    if (width == 0 || height == 0)
        return Support::Unexpected(std::string("the shared texture has no pixels"));
    if (handle != handle_ || width != width_ || height != height_) {
        opened_.Reset();
        staging_.Reset();
        auto* shared = std::bit_cast<HANDLE>(static_cast<uintptr_t>(handle));
        if (FAILED(device_->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                          D3DPOOL_DEFAULT, &opened_, &shared)))
            return Support::Unexpected(std::string("the host's texture would not open"));
        if (FAILED(device_->CreateOffscreenPlainSurface(width, height, D3DFMT_A8R8G8B8,
                                                        D3DPOOL_SYSTEMMEM, &staging_, nullptr)))
            return Support::Unexpected(std::string("no system memory surface for the frame"));
        handle_ = handle;
        width_ = width;
        height_ = height;
    }
    ComPtr<IDirect3DSurface9> level;
    if (FAILED(opened_->GetSurfaceLevel(0, &level)))
        return Support::Unexpected(std::string("the host's texture has no surface"));
    if (FAILED(device_->GetRenderTargetData(level, staging_)))
        return Support::Unexpected(std::string("the frame would not copy to system memory"));
    D3DLOCKED_RECT locked = {};
    if (FAILED(staging_->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
        return Support::Unexpected(std::string("the frame would not lock"));
    const std::size_t row = static_cast<std::size_t>(width) * kBytesPerPixel;
    std::vector<uint8_t> pixels(row * height);
    const auto* source = static_cast<const uint8_t*>(locked.pBits);
    for (uint32_t y = 0; y < height; y++) {
        std::memcpy(pixels.data() + (row * y),
                    source + (static_cast<std::size_t>(locked.Pitch) * y), row);
    }
    staging_->UnlockRect();
    return pixels;
}

}
