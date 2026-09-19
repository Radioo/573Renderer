#pragma once

#include "document/clip.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <string>

namespace Document {

[[nodiscard]] Support::Expected<void, std::string>
SplitSpan(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth, uint32_t frame);

}
