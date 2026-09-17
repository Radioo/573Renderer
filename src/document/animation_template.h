#pragma once

#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

struct AnimationTemplate {
    AfpAnimation::Animation animation;
    std::vector<uint16_t> shapes;
};

[[nodiscard]] Support::Expected<AnimationTemplate, std::string>
EmptyLike(const AfpAnimation::Animation& like, std::string_view name, uint32_t frames);

}
