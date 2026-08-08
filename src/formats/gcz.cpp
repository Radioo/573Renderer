#include "formats/gcz.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Gcz {

namespace {

constexpr size_t kHeaderBytes = 24;
constexpr uint8_t kExpand5 = 255 / 31;

uint32_t Be32(std::span<const uint8_t> b, size_t o) {
    return ((uint32_t)b[o] << 24) | ((uint32_t)b[o + 1] << 16) | ((uint32_t)b[o + 2] << 8) |
           (uint32_t)b[o + 3];
}

uint16_t Be16(std::span<const uint8_t> b, size_t o) {
    return (uint16_t)(((uint32_t)b[o] << 8) | (uint32_t)b[o + 1]);
}

uint8_t Expand5(uint32_t v) {
    return (uint8_t)((v * 255U + 15U) / 31U);
}

}

bool Parse(std::span<const uint8_t> payload, Tile& out, std::string& err) {
    if (payload.size() < kHeaderBytes) {
        err = "buffer smaller than the 24-byte GC header";
        return false;
    }
    if (payload[0] != 'G' || payload[1] != 'C' || payload[2] != ' ') {
        err = "not GC texture data";
        return false;
    }
    out.width = Be16(payload, 12);
    out.height = Be16(payload, 14);
    const uint32_t declared = Be32(payload, 20);
    const size_t need = (size_t)out.width * (size_t)out.height * 2;
    if (out.width <= 0 || out.height <= 0) {
        err = "GC tile has a zero dimension";
        return false;
    }
    if (declared != need) {
        err = "GC payload size " + std::to_string(declared) + " disagrees with " +
              std::to_string(out.width) + "x" + std::to_string(out.height) + " at 16bpp";
        return false;
    }
    if (payload.size() < kHeaderBytes + need) {
        err = "GC pixel data runs past the buffer";
        return false;
    }
    out.argb1555.assign(payload.begin() + (ptrdiff_t)kHeaderBytes,
                        payload.begin() + (ptrdiff_t)(kHeaderBytes + need));
    return true;
}

void ExpandToBgra(const Tile& tile, std::vector<uint8_t>& out) {
    const size_t px = (size_t)tile.width * (size_t)tile.height;
    out.assign(px * 4, 0);
    for (size_t i = 0; i < px; i++) {
        const uint32_t lo = tile.argb1555[i * 2];
        const uint32_t hi = tile.argb1555[(i * 2) + 1];
        const uint32_t v = lo | (hi << 8);
        out[(i * 4) + 0] = Expand5(v & 0x1FU);
        out[(i * 4) + 1] = Expand5((v >> 5U) & 0x1FU);
        out[(i * 4) + 2] = Expand5((v >> 10U) & 0x1FU);
        out[(i * 4) + 3] = ((v & 0x8000U) != 0) ? 255 : 0;
    }
    (void)kExpand5;
}

}
