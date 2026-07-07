#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Loop {

using FrameBuffer = std::vector<std::vector<uint8_t>>;

struct BlendPlan {
    int loop_length = 0;
    int crossfade = 0;
    double best_mad = 0.0;
};

[[nodiscard]] BlendPlan PlanBlendLoop(FrameBuffer& buf, int blend_frames);

void ComposeBlendFrame(const FrameBuffer& buf, const BlendPlan& plan, int index,
                       std::span<uint8_t> out);

}
