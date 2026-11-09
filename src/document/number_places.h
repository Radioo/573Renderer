#pragma once

#include "document/clip.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <string>

namespace Document {

inline constexpr uint32_t kMostNumberPlaces = 7;

enum class NumberGrows : uint8_t { Right, Left };

struct NumberSpread {
    ClipId clip;
    std::string name;
    uint16_t depth = 0;
    uint32_t frame = 0;
    uint32_t places = 0;
    double advance = 0;
    NumberGrows grows = NumberGrows::Right;
};

[[nodiscard]] std::string PlaceName(const std::string& stem, uint32_t places, uint32_t at);

[[nodiscard]] Support::Expected<void, std::string>
SpreadIntoPlaces(AfpAnimation::Animation& animation, const NumberSpread& spread);

}
