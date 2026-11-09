#include "document/timeline_snap.h"

#include "document/timeline.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <vector>

namespace Document {

namespace {

std::optional<int64_t> NearestPull(int64_t edge, const std::vector<uint32_t>& targets,
                                   uint32_t reach) {
    std::optional<int64_t> pull;
    for (const uint32_t target : targets) {
        const int64_t to = static_cast<int64_t>(target) - edge;
        if (std::abs(to) > static_cast<int64_t>(reach)) continue;
        if (!pull || std::abs(to) < std::abs(*pull)) pull = to;
    }
    return pull;
}

}

std::vector<uint32_t> SnapTargets(const std::vector<DepthRow>& rows, uint16_t depth,
                                  const Span& dragged, const std::vector<uint32_t>& marks) {
    std::vector<uint32_t> targets = marks;
    for (const DepthRow& row : rows) {
        for (const Span& span : row.spans) {
            if (row.depth == depth && span == dragged) continue;
            targets.push_back(span.first_frame);
            targets.push_back(span.last_frame + 1);
        }
    }
    std::ranges::sort(targets);
    const auto [repeated, end] = std::ranges::unique(targets);
    targets.erase(repeated, end);
    return targets;
}

int64_t SnapShift(const Span& span, int64_t by, const std::vector<uint32_t>& targets,
                  uint32_t reach) {
    const std::optional<int64_t> start =
        NearestPull(static_cast<int64_t>(span.first_frame) + by, targets, reach);
    const std::optional<int64_t> end =
        NearestPull(static_cast<int64_t>(span.last_frame) + 1 + by, targets, reach);
    if (start && (!end || std::abs(*start) <= std::abs(*end))) return by + *start;
    if (end) return by + *end;
    return by;
}

uint32_t SnapEdge(uint32_t edge, const std::vector<uint32_t>& targets, uint32_t reach) {
    const std::optional<int64_t> pull = NearestPull(edge, targets, reach);
    return pull ? static_cast<uint32_t>(static_cast<int64_t>(edge) + *pull) : edge;
}

}
