#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace Render {

struct HsvDescriptor {
    uint8_t id = 0;
    uint16_t valid = 0;
    float hue = 0.0F;
    float sat = 0.0F;
    float val = 0.0F;
};

[[nodiscard]] HsvDescriptor ParseHsvDescriptor(std::span<const unsigned char> raw);

[[nodiscard]] bool HsvFilterActive(const HsvDescriptor& d);

[[nodiscard]] std::array<float, 4> HsvShaderConstant(const HsvDescriptor& d);

}
