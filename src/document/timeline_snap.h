#pragma once

#include "document/timeline.h"

#include <cstdint>
#include <vector>

namespace Document {

[[nodiscard]] std::vector<uint32_t> SnapTargets(const std::vector<DepthRow>& rows, uint16_t depth,
                                                const Span& dragged,
                                                const std::vector<uint32_t>& marks);

[[nodiscard]] int64_t SnapShift(const Span& span, int64_t by, const std::vector<uint32_t>& targets,
                                uint32_t reach);

[[nodiscard]] uint32_t SnapEdge(uint32_t edge, const std::vector<uint32_t>& targets,
                                uint32_t reach);

}
