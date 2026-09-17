#pragma once

#include "formats/afp_animation.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Document {

enum class CharacterKind : uint8_t { Sprite, Image, Shape, Imported };

struct CharacterSummary {
    uint16_t id = 0;
    CharacterKind kind = CharacterKind::Sprite;
    std::string label;
};

[[nodiscard]] std::vector<CharacterSummary>
Characters(const AfpAnimation::Animation& animation,
           const std::map<uint16_t, std::string>& shape_images);

}
