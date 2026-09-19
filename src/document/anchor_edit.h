#pragma once

#include "document/clip.h"
#include "document/stage_bounds.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <map>
#include <string>

namespace Document {

[[nodiscard]] Support::Expected<void, std::string>
CentreAnchor(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth, uint32_t frame,
             const std::map<uint16_t, Box>& shape_bounds);

}
