#pragma once

#include "formats/afp_animation.h"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace Document {

struct AppliedState {
    std::array<double, 6> matrix{1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
    std::array<double, 4> multiply{1.0, 1.0, 1.0, 1.0};
    std::array<double, 4> add{0.0, 0.0, 0.0, 0.0};

    friend bool operator==(const AppliedState&, const AppliedState&) = default;
};

void ApplyPlacement(AppliedState& state, const AfpAnimation::Placement& placement);

[[nodiscard]] std::vector<std::pair<uint32_t, AppliedState>>
ReplayDepth(const AfpAnimation::Container& clip, uint16_t depth, uint32_t first, uint32_t last);

}
