#pragma once

#include "support/com_ptr.h"
#include "support/expected.h"

#include <windows.h>

#include <d3d9.h>

#include <string>

namespace SharedFrame {

struct Target {
    ComPtr<IDirect3DTexture9> texture;
    ComPtr<IDirect3DSurface9> sync_probe;
    HANDLE handle = nullptr;
    int width = 0;
    int height = 0;
};

[[nodiscard]] Support::Expected<Target, std::string> Create(IDirect3DDevice9* device, int width,
                                                            int height);

[[nodiscard]] Support::Expected<void, std::string>
Copy(IDirect3DDevice9* device, IDirect3DSurface9* source, const Target& target);

}
