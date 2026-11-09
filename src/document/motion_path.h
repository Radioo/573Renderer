#pragma once

#include "document/keyframes.h"
#include "document/stage_bounds.h"
#include "formats/afp_animation.h"

#include <cstdint>
#include <vector>

namespace Document {

struct PathPoint {
    uint32_t frame = 0;
    Point at{};
    bool keyed = false;

    friend bool operator==(const PathPoint&, const PathPoint&) = default;
};

[[nodiscard]] std::vector<PathPoint> MotionPath(const AfpAnimation::Container& clip, uint16_t depth,
                                                uint32_t frame, const std::vector<Track>& tracks);

}
