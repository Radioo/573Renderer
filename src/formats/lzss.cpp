#include "formats/lzss.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Lzss {

namespace {

constexpr size_t kRingInit = 4078;
constexpr size_t kMinMatch = 3;
constexpr uint32_t kFlagsExhausted = 0x100;
constexpr uint32_t kFlagsRefill = 0xFF00;

uint32_t ReadSizeLe(std::span<const uint8_t> blob) {
    return (uint32_t)blob[0] | ((uint32_t)blob[1] << 8) | ((uint32_t)blob[2] << 16) |
           ((uint32_t)blob[3] << 24);
}

}

bool Decompress(std::span<const uint8_t> blob, std::vector<uint8_t>& out, std::string& err) {
    if (blob.size() < 4) {
        err = "buffer smaller than the 4-byte size prefix";
        return false;
    }
    const uint32_t usize = ReadSizeLe(blob);

    std::vector<uint8_t> work(kRingBytes + usize, 0);
    size_t src = 4;
    size_t dst = kRingBytes;
    const size_t end = kRingBytes + usize;
    size_t ring = kRingInit;
    uint32_t flags = 0;

    while (dst < end) {
        flags >>= 1;
        if ((flags & kFlagsExhausted) == 0) {
            if (src >= blob.size()) break;
            flags = (uint32_t)blob[src++] | kFlagsRefill;
        }
        if ((flags & 1) != 0) {
            if (src >= blob.size()) break;
            work[dst++] = blob[src++];
            ring = (ring + 1) & (kRingBytes - 1);
            continue;
        }
        if (src + 1 >= blob.size()) break;
        const uint32_t b0 = blob[src];
        const uint32_t b1 = blob[src + 1];
        src += 2;
        const size_t pos = ((b1 & 0xF0U) << 4U) | b0;
        const size_t len = (b1 & 0x0FU) + kMinMatch;
        auto off = (ptrdiff_t)pos - (ptrdiff_t)ring;
        if (off >= 0) off -= (ptrdiff_t)kRingBytes;
        for (size_t i = 0; i < len && dst < end; i++) {
            work[dst] = work[(size_t)((ptrdiff_t)dst + off)];
            dst++;
        }
        ring = (ring + len) & (kRingBytes - 1);
    }

    if (dst != end) {
        err = "compressed stream ended after " + std::to_string(dst - kRingBytes) + " of " +
              std::to_string(usize) + " bytes";
        return false;
    }
    out.assign(work.begin() + (ptrdiff_t)kRingBytes, work.end());
    return true;
}

}
