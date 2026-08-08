#pragma once

#include <d3d9.h>
#include <cstdint>

namespace Render {

struct VtxLayout {
    bool has_uv = false;
    bool skip2 = false;
    bool has_vcol = false;
    bool pos3 = false;
    bool pos2 = false;
    int stride = 0;
};

VtxLayout DecodeVtxLayout(int flags);

D3DPRIMITIVETYPE MapPrimType(int afp_type, int n, int& prims);

uint32_t MulARGB(uint32_t a, uint32_t b);

}
