#include "document/stage_align.h"

#include "document/stage_bounds.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace Document {

namespace {

struct Extent {
    double low = 0;
    double high = 0;

    [[nodiscard]] double Middle() const { return (low + high) / 2; }
};

Extent ExtentOf(const StageOutline& outline, std::size_t axis) {
    Extent extent{.low = outline.corners[0].at(axis), .high = outline.corners[0].at(axis)};
    for (const Point& corner : outline.corners) {
        extent.low = std::min(extent.low, corner.at(axis));
        extent.high = std::max(extent.high, corner.at(axis));
    }
    return extent;
}

double Line(const Extent& extent, AlignTo how) {
    switch (how) {
    case AlignTo::Left:
    case AlignTo::Top:
        return extent.low;
    case AlignTo::Right:
    case AlignTo::Bottom:
        return extent.high;
    case AlignTo::HorizontalCentre:
    case AlignTo::VerticalCentre:
        return extent.Middle();
    }
    return extent.low;
}

Point Along(std::size_t axis, double distance) {
    return axis == 0 ? Point{distance, 0} : Point{0, distance};
}

}

std::vector<DepthOffset> AlignOffsets(const std::vector<StageOutline>& chosen, AlignTo how) {
    if (chosen.empty()) return {};
    const bool across =
        how == AlignTo::Left || how == AlignTo::HorizontalCentre || how == AlignTo::Right;
    const std::size_t axis = across ? 0 : 1;
    Extent all = ExtentOf(chosen.front(), axis);
    for (const StageOutline& outline : chosen) {
        const Extent one = ExtentOf(outline, axis);
        all.low = std::min(all.low, one.low);
        all.high = std::max(all.high, one.high);
    }
    const double target = Line(all, how);
    std::vector<DepthOffset> offsets;
    offsets.reserve(chosen.size());
    for (const StageOutline& outline : chosen) {
        offsets.push_back(
            DepthOffset{.depth = outline.depth,
                        .offset = Along(axis, target - Line(ExtentOf(outline, axis), how))});
    }
    return offsets;
}

std::vector<DepthOffset> SpreadOffsets(const std::vector<StageOutline>& chosen, Spread how) {
    const std::size_t axis = how == Spread::Across ? 0 : 1;
    std::vector<const StageOutline*> order;
    order.reserve(chosen.size());
    for (const StageOutline& outline : chosen)
        order.push_back(&outline);
    std::ranges::stable_sort(order, {}, [axis](const StageOutline* outline) {
        return ExtentOf(*outline, axis).Middle();
    });
    std::vector<DepthOffset> offsets;
    if (order.size() < 3) return offsets;
    const double first = ExtentOf(*order.front(), axis).Middle();
    const double step =
        (ExtentOf(*order.back(), axis).Middle() - first) / static_cast<double>(order.size() - 1);
    for (std::size_t at = 0; at < order.size(); at++) {
        const double wanted = first + (step * static_cast<double>(at));
        offsets.push_back(
            DepthOffset{.depth = order[at]->depth,
                        .offset = Along(axis, wanted - ExtentOf(*order[at], axis).Middle())});
    }
    return offsets;
}

}
