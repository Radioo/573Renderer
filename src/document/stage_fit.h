#pragma once

#include "document/stage_bounds.h"
#include "document/stage_move.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <map>
#include <string>

namespace Document {

enum class StageFit : uint8_t { Both, Width, Height };

struct FitChange {
    Reshape reshape;
    StageOffset offset;
};

[[nodiscard]] Support::Expected<FitChange, std::string>
FitToStage(const AfpAnimation::Animation& animation, uint16_t depth, uint32_t frame,
           const std::map<uint16_t, Box>& shape_bounds, StageFit fit);

}
