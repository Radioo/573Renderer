#pragma once

#include "document/clip.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <string>

namespace Document {

struct GroupRange {
    ClipId clip;
    uint16_t first_depth = 0;
    uint16_t last_depth = 0;
    uint32_t first_frame = 0;
    uint32_t last_frame = 0;
};

[[nodiscard]] Support::Expected<uint16_t, std::string>
GroupIntoSprite(AfpAnimation::Animation& animation, const GroupRange& range);

[[nodiscard]] Support::Expected<void, std::string>
UngroupSprite(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth, uint32_t frame);

}
