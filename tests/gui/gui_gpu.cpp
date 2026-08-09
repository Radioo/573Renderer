#include "gui_gpu.h"

#include "app_globals.h"
#include "warp_device.h"

namespace GuiTest {

WarpGpu::WarpGpu() : previous_(g_d3d.device) {
    if (WarpD3D9::Create(warp_, 256, 256)) g_d3d.device = warp_.device;
}

WarpGpu::~WarpGpu() {
    g_d3d.device = previous_;
}

}
