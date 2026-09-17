#include "document/stage_snap.h"

#include "document/stage_bounds.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace Document {

namespace {

using Lines = std::array<double, 3>;

Lines LinesOf(const StageOutline& outline, std::size_t axis, double shift) {
    double low = outline.corners[0].at(axis);
    double high = low;
    for (const Point& corner : outline.corners) {
        low = std::min(low, corner.at(axis));
        high = std::max(high, corner.at(axis));
    }
    return {low + shift, ((low + high) / 2) + shift, high + shift};
}

struct Match {
    double target = 0;
    double moved = 0;
};

std::optional<Match> Nearest(const Lines& moving, const std::vector<double>& targets,
                             double reach) {
    std::optional<Match> best;
    for (const double target : targets) {
        for (const double line : moving) {
            const double distance = std::abs(target - line);
            if (distance > reach) continue;
            if (best && distance >= std::abs(best->target - best->moved)) continue;
            best = Match{.target = target, .moved = line};
        }
    }
    return best;
}

}

Snapped SnapMove(const StageOutline& moving, const std::vector<StageOutline>& others, Point stage,
                 Point offset, double reach) {
    Snapped snapped{.offset = offset, .guides = {}};
    for (std::size_t axis = 0; axis < 2; axis++) {
        std::vector<double> targets{0, stage.at(axis) / 2, stage.at(axis)};
        for (const StageOutline& other : others) {
            if (other.depth == moving.depth) continue;
            const Lines lines = LinesOf(other, axis, 0);
            targets.insert(targets.end(), lines.begin(), lines.end());
        }
        const std::optional<Match> match =
            Nearest(LinesOf(moving, axis, offset.at(axis)), targets, reach);
        if (!match) continue;
        snapped.offset.at(axis) += match->target - match->moved;
        snapped.guides.push_back(SnapGuide{.vertical = axis == 0, .at = match->target});
    }
    return snapped;
}

}
