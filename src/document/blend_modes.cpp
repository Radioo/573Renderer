#include "document/blend_modes.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace Document {

namespace {

constexpr std::array<BlendMode, 9> kModes{{
    {.value = 0, .name = "Normal"},
    {.value = 3, .name = "Multiply"},
    {.value = 4, .name = "Additive"},
    {.value = 5, .name = "Lighten"},
    {.value = 6, .name = "Darken"},
    {.value = 8, .name = "Additive, second code"},
    {.value = 9, .name = "Subtractive"},
    {.value = 0x46, .name = "Subtractive, second code"},
    {.value = 0x4F, .name = "Additive, third code"},
}};

}

std::span<const BlendMode> BlendModes() {
    return kModes;
}

std::string BlendName(uint8_t value) {
    const auto found = std::ranges::find(kModes, value, &BlendMode::value);
    if (found == kModes.end()) return "Mode " + std::to_string(value);
    return std::string(found->name);
}

}
