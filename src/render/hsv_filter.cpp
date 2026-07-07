#include "render/hsv_filter.h"

#include <array>
#include <cstring>
#include <span>

namespace Render {

HsvDescriptor ParseHsvDescriptor(std::span<const unsigned char> raw) {
    HsvDescriptor d;
    if (raw.size() < 16) return d;
    d.id = raw[0];
    std::memcpy(&d.valid, &raw[2], sizeof(d.valid));
    std::memcpy(&d.hue, &raw[4], sizeof(d.hue));
    std::memcpy(&d.sat, &raw[8], sizeof(d.sat));
    std::memcpy(&d.val, &raw[12], sizeof(d.val));
    return d;
}

bool HsvFilterActive(const HsvDescriptor& d) {
    return d.valid == 1 && (d.id == 100 || d.id == 101);
}

std::array<float, 4> HsvShaderConstant(const HsvDescriptor& d) {
    return {360.0F - d.hue, (d.sat * 0.01F) + 1.0F, (d.val * 0.01F) + 1.0F, 0.0F};
}

}
