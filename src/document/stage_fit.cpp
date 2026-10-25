#include "document/stage_fit.h"

#include "document/stage_bounds.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Document {

namespace {

constexpr std::size_t kMinX = 0;
constexpr std::size_t kMaxX = 1;
constexpr std::size_t kMinY = 2;
constexpr std::size_t kMaxY = 3;
constexpr double kSmallest = 1e-9;

Box BoxAround(const StageOutline& outline) {
    Box box{.left = outline.corners[0][0],
            .right = outline.corners[0][0],
            .top = outline.corners[0][1],
            .bottom = outline.corners[0][1]};
    for (const Point& corner : outline.corners) {
        box.left = std::min(box.left, corner[0]);
        box.right = std::max(box.right, corner[0]);
        box.top = std::min(box.top, corner[1]);
        box.bottom = std::max(box.bottom, corner[1]);
    }
    return box;
}

}

Support::Expected<FitChange, std::string> FitToStage(const AfpAnimation::Animation& animation,
                                                     uint16_t depth, uint32_t frame,
                                                     const std::map<uint16_t, Box>& shape_bounds,
                                                     StageFit fit) {
    const std::string named = "depth " + std::to_string(depth);
    const std::vector<StageOutline> outlines = StageOutlines(animation, {}, frame, shape_bounds);
    const auto found = std::ranges::find(outlines, depth, &StageOutline::depth);
    if (found == outlines.end()) {
        return Support::Unexpected(named + " shows nothing with a box the editor knows on frame " +
                                   std::to_string(frame));
    }
    if (std::abs(found->linear.b) > kSmallest || std::abs(found->linear.c) > kSmallest) {
        return Support::Unexpected(named + " is turned or skewed, so its box does not line up " +
                                   "with the stage");
    }
    const Box box = BoxAround(*found);
    const double width = box.right - box.left;
    const double height = box.bottom - box.top;
    if (width < kSmallest || height < kSmallest)
        return Support::Unexpected(named + " is flat, so no scale can make it fill the stage");
    const double stage_width = animation.rect[kMaxX] - animation.rect[kMinX];
    const double stage_height = animation.rect[kMaxY] - animation.rect[kMinY];
    double scale_x = stage_width / width;
    double scale_y = stage_height / height;
    if (fit == StageFit::Width) scale_y = scale_x;
    if (fit == StageFit::Height) scale_x = scale_y;
    const Point& anchor = found->anchor;
    const double centre_x = anchor[0] + ((((box.left + box.right) / 2) - anchor[0]) * scale_x);
    const double centre_y = anchor[1] + ((((box.top + box.bottom) / 2) - anchor[1]) * scale_y);
    return FitChange{
        .reshape = {.scale_x = scale_x, .scale_y = scale_y, .turn = 0},
        .offset = {.x = (stage_width / 2) - centre_x, .y = (stage_height / 2) - centre_y}};
}

}
