#include "render/prim_layout.h"

#include <cstdint>

namespace Render {

VtxLayout DecodeVtxLayout(int flags) {
    VtxLayout l{};
    l.has_uv = (flags & 0x08) != 0;
    l.skip2 = (flags & 0x10) != 0;
    l.has_vcol = (flags & 0x04) != 0;
    l.pos3 = (flags & 0x02) != 0;
    l.pos2 = (flags & 0x01) != 0;
    int pos_n = 0;
    if (l.pos3) {
        pos_n = 3;
    } else if (l.pos2) {
        pos_n = 2;
    }
    l.stride = (l.has_uv ? 2 : 0) + (l.skip2 ? 2 : 0) + (l.has_vcol ? 1 : 0) + pos_n;
    return l;
}

D3DPRIMITIVETYPE MapPrimType(int afp_type, int n, int& prims) {
    D3DPRIMITIVETYPE pt = D3DPT_TRIANGLELIST;
    prims = 0;
    switch (afp_type) {
    case 1:
    case 3:
        pt = D3DPT_LINELIST;
        prims = n / 2;
        break;
    case 2:
        pt = D3DPT_LINESTRIP;
        prims = n - 1;
        break;
    case 4:
        pt = D3DPT_TRIANGLELIST;
        prims = n / 3;
        break;
    case 5:
        pt = D3DPT_TRIANGLESTRIP;
        prims = n - 2;
        break;
    case 6:
        pt = D3DPT_TRIANGLEFAN;
        prims = n - 2;
        break;
    default:
        pt = D3DPT_POINTLIST;
        prims = n;
        break;
    }
    return pt;
}

uint32_t MulARGB(uint32_t a, uint32_t b) {
    uint32_t o = 0;
    for (int sh = 0; sh < 32; sh += 8) {
        uint32_t const ca = (a >> sh) & 0xFF;
        uint32_t const cb = (b >> sh) & 0xFF;
        o |= ((ca * cb + 127) / 255) << sh;
    }
    return o;
}

}
