#pragma once

#include "document/stage_bounds.h"

#include <vector>

namespace Document {

struct SnapGuide {
    bool vertical = false;
    double at = 0;

    friend bool operator==(const SnapGuide&, const SnapGuide&) = default;
};

struct Snapped {
    Point offset{};
    std::vector<SnapGuide> guides;
};

[[nodiscard]] Snapped SnapMove(const StageOutline& moving, const std::vector<StageOutline>& others,
                               Point stage, Point offset, double reach);

}
