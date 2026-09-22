#pragma once

#include "document/stage_bounds.h"

#include <cstdint>
#include <vector>

namespace Document {

enum class AlignTo : uint8_t { Left, HorizontalCentre, Right, Top, VerticalCentre, Bottom };

enum class Spread : uint8_t { Across, Down };

struct DepthOffset {
    uint16_t depth = 0;
    Point offset{};

    friend bool operator==(const DepthOffset&, const DepthOffset&) = default;
};

[[nodiscard]] std::vector<DepthOffset> AlignOffsets(const std::vector<StageOutline>& chosen,
                                                    AlignTo how);

[[nodiscard]] std::vector<DepthOffset> SpreadOffsets(const std::vector<StageOutline>& chosen,
                                                     Spread how);

}
