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

struct Linear {
    double a = 1;
    double b = 0;
    double c = 0;
    double d = 1;

    friend bool operator==(const Linear&, const Linear&) = default;
};

struct StageOutline {
    uint16_t depth = 0;
    std::array<Point, 4> corners{};
    Point anchor{};
    Linear linear;

    friend bool operator==(const StageOutline&, const StageOutline&) = default;
};

[[nodiscard]] std::vector<StageOutline> StageOutlines(const AfpAnimation::Animation& animation,
                                                      ClipId clip, uint32_t frame,
                                                      const std::map<uint16_t, Box>& shape_bounds);

struct Reshape {
    double scale_x = 1;
    double scale_y = 1;
    double turn = 0;
};

[[nodiscard]] Linear Reshaped(const Linear& linear, const Reshape& reshape);

[[nodiscard]] StageOutline ReshapedOutline(const StageOutline& outline, const Reshape& reshape);

[[nodiscard]] Reshape ScaleToReach(const StageOutline& outline, Point from, Point to);

[[nodiscard]] Reshape TurnToReach(const StageOutline& outline, Point from, Point to);

[[nodiscard]] std::optional<uint16_t> DepthAt(const std::vector<StageOutline>& outlines,
                                              Point point);

}
