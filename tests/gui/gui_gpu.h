#pragma once

#include "warp_device.h"

#include <d3d9.h>

namespace GuiTest {

class WarpGpu {
public:
    WarpGpu();
    ~WarpGpu();
    WarpGpu(const WarpGpu&) = delete;
    WarpGpu& operator=(const WarpGpu&) = delete;
    WarpGpu(WarpGpu&&) = delete;
    WarpGpu& operator=(WarpGpu&&) = delete;

    [[nodiscard]] bool ok() const { return warp_.ok; }

private:
    WarpD3D9::Device warp_;
    IDirect3DDevice9* previous_ = nullptr;
};

}
