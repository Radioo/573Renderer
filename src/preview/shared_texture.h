#pragma once

#include "support/com_ptr.h"
#include "support/expected.h"

#include <windows.h>

#include <d3d9.h>

#include <cstdint>
#include <string>
#include <vector>

namespace SharedTexture {

class Reader {
public:
    [[nodiscard]] static Support::Expected<Reader, std::string> Create();

    [[nodiscard]] Support::Expected<std::vector<uint8_t>, std::string>
    Read(uint64_t handle, uint32_t width, uint32_t height);

private:
    ComPtr<IDirect3D9Ex> d3d_;
    ComPtr<IDirect3DDevice9Ex> device_;
    ComPtr<IDirect3DTexture9> opened_;
    ComPtr<IDirect3DSurface9> staging_;
    uint64_t handle_ = 0;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

}
