#pragma once

#include "document/clip.h"
#include "formats/afp_animation.h"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace Document {

struct Box {
    double left = 0;
    double right = 0;
    double top = 0;
    double bottom = 0;

    friend bool operator==(const Box&, const Box&) = default;
};

using Point = std::array<double, 2>;

struct StageOutline {
    uint16_t depth = 0;
    std::array<Point, 4> corners{};

    friend bool operator==(const StageOutline&, const StageOutline&) = default;
};

[[nodiscard]] std::vector<StageOutline> StageOutlines(const AfpAnimation::Animation& animation,
                                                      ClipId clip, uint32_t frame,
                                                      const std::map<uint16_t, Box>& shape_bounds);

[[nodiscard]] std::optional<uint16_t> DepthAt(const std::vector<StageOutline>& outlines,
                                              Point point);

}
