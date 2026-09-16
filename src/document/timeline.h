#pragma once

#include "formats/afp_animation.h"

#include <cstdint>
#include <vector>

namespace Document {

struct Span {
    uint32_t first_frame = 0;
    uint32_t last_frame = 0;
};

struct DepthRow {
    uint16_t depth = 0;
    std::vector<Span> spans;
};

[[nodiscard]] std::vector<DepthRow> DepthRows(const AfpAnimation::Container& clip);

}
