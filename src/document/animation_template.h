#pragma once

#include "formats/afp_animation.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace Document {

struct AnimationTemplate {
    AfpAnimation::Animation animation;
    std::vector<uint16_t> shapes;
};

[[nodiscard]] AnimationTemplate EmptyLike(const AfpAnimation::Animation& like,
                                          std::string_view name, uint32_t frames);

}
