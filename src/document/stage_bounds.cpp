#include "document/stage_bounds.h"

#include "document/clip.h"
#include "document/placement_effect.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr double kPixelsPerUnit = 0.05;
constexpr double kSmallest = 1e-9;
constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kThreeD = 0x04000000;
constexpr std::size_t kScaleX = 0;
constexpr std::size_t kSkewB = 1;
constexpr std::size_t kSkewC = 2;
constexpr std::size_t kScaleY = 3;
constexpr std::size_t kMoveX = 4;
constexpr std::size_t kMoveY = 5;

struct LiveObject {
    std::optional<uint16_t> character;
    AppliedState state;
    Point origin{};
    bool three_d = false;
};

using LiveObjects = std::map<uint16_t, LiveObject>;

void PlaceOn(LiveObjects& live, const AfpAnimation::Placement& placement) {
    if ((placement.flags & kUpdateExisting) == 0) {
        live[placement.depth] = LiveObject{};
    } else if (!live.contains(placement.depth)) {
        return;
    }
    LiveObject& object = live[placement.depth];
    if (placement.character) object.character = *placement.character;
    ApplyPlacement(object.state, placement);
    if ((placement.flags & kThreeD) != 0) object.three_d = true;
    if (placement.origin) {
        object.origin = placement.geometry ? Point{0.0, 0.0}
                                           : Point{(*placement.origin)[0] * kPixelsPerUnit,
                                                   (*placement.origin)[1] * kPixelsPerUnit};
    }
}

void Step(LiveObjects& live, const AfpAnimation::Tag& tag) {
    if (const auto* remove = std::get_if<AfpAnimation::Remove>(&tag.body)) {
        live.erase(remove->depth);
        return;
    }
    if (const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body))
        PlaceOn(live, *placement);
}

void ForEachFrame(const AfpAnimation::Container& clip, uint32_t last,
                  const std::function<void(const LiveObjects&)>& visit) {
    LiveObjects live;
    for (uint32_t frame = 0; frame <= last && frame < clip.frames.size(); frame++) {
        const AfpAnimation::Frame& owner = clip.frames[frame];
        for (std::size_t i = 0; i < owner.tag_count; i++) {
            const std::size_t index = owner.first_tag + i;
            if (index >= clip.tags.size()) break;
            Step(live, clip.tags[index]);
        }
        visit(live);
    }
}

Point Place(const LiveObject& object, Point point) {
    const std::array<double, 6>& m = object.state.matrix;
    const double x = point[0] - object.origin[0];
    const double y = point[1] - object.origin[1];
    return {(m[kScaleX] * x) + (m[kSkewC] * y) + (m[kMoveX] * kPixelsPerUnit),
            (m[kSkewB] * x) + (m[kScaleY] * y) + (m[kMoveY] * kPixelsPerUnit)};
}

std::array<Point, 4> Corners(const Box& box) {
    return {Point{box.left, box.top}, Point{box.right, box.top}, Point{box.right, box.bottom},
            Point{box.left, box.bottom}};
}

void Extend(std::optional<Box>& total, Point point) {
    if (!total) {
        total = Box{.left = point[0], .right = point[0], .top = point[1], .bottom = point[1]};
        return;
    }
    total->left = std::min(total->left, point[0]);
    total->right = std::max(total->right, point[0]);
    total->top = std::min(total->top, point[1]);
    total->bottom = std::max(total->bottom, point[1]);
}

class CharacterBounds {
public:
    CharacterBounds(const AfpAnimation::Animation& animation, const std::map<uint16_t, Box>& shapes)
        : animation_(&animation), shapes_(&shapes) {}

    std::optional<Box> Of(uint16_t character) {
        const auto shape = shapes_->find(character);
        if (shape != shapes_->end()) return shape->second;
        const auto known = sprites_.find(character);
        if (known != sprites_.end()) return known->second;
        const AfpAnimation::Container* sprite = FindClip(*animation_, ClipId{.sprite = character});
        if (sprite == nullptr || !visiting_.insert(character).second) return std::nullopt;
        std::optional<Box> total;
        ForEachFrame(*sprite, static_cast<uint32_t>(sprite->frames.size()),
                     [this, &total](const LiveObjects& live) { AddLive(live, total); });
        visiting_.erase(character);
        sprites_[character] = total;
        return total;
    }

private:
    void AddLive(const LiveObjects& live, std::optional<Box>& total) {
        for (const auto& [depth, object] : live) {
            if (object.three_d || !object.character) continue;
            const std::optional<Box> inner = Of(*object.character);
            if (!inner) continue;
            for (const Point& corner : Corners(*inner))
                Extend(total, Place(object, corner));
        }
    }

    const AfpAnimation::Animation* animation_;
    const std::map<uint16_t, Box>* shapes_;
    std::map<uint16_t, std::optional<Box>> sprites_;
    std::set<uint16_t> visiting_;
};

std::optional<Point> ToLocal(const StageOutline& outline, Point point) {
    const Linear& m = outline.linear;
    const double determinant = (m.a * m.d) - (m.b * m.c);
    if (std::abs(determinant) < kSmallest) return std::nullopt;
    const double x = point[0] - outline.anchor[0];
    const double y = point[1] - outline.anchor[1];
    return Point{((x * m.d) - (y * m.c)) / determinant, ((y * m.a) - (x * m.b)) / determinant};
}

Point FromLocal(const StageOutline& outline, Point local) {
    const Linear& m = outline.linear;
    return {outline.anchor[0] + (m.a * local[0]) + (m.c * local[1]),
            outline.anchor[1] + (m.b * local[0]) + (m.d * local[1])};
}

double Cross(Point from, Point to, Point point) {
    return ((to[0] - from[0]) * (point[1] - from[1])) - ((to[1] - from[1]) * (point[0] - from[0]));
}

bool Inside(const StageOutline& outline, Point point) {
    bool below = false;
    bool above = false;
    for (std::size_t i = 0; i < outline.corners.size(); i++) {
        const double side = Cross(outline.corners.at(i),
                                  outline.corners.at((i + 1) % outline.corners.size()), point);
        if (side < 0) below = true;
        if (side > 0) above = true;
    }
    return !(below && above);
}

}

std::vector<StageOutline> StageOutlines(const AfpAnimation::Animation& animation, ClipId clip,
                                        uint32_t frame,
                                        const std::map<uint16_t, Box>& shape_bounds) {
    std::vector<StageOutline> outlines;
    const AfpAnimation::Container* shown = FindClip(animation, clip);
    if (shown == nullptr || frame >= shown->frames.size()) return outlines;
    LiveObjects at_frame;
    ForEachFrame(*shown, frame, [&at_frame](const LiveObjects& live) { at_frame = live; });
    CharacterBounds bounds(animation, shape_bounds);
    for (const auto& [depth, object] : at_frame) {
        if (object.three_d || !object.character) continue;
        const std::optional<Box> box = bounds.Of(*object.character);
        if (!box) continue;
        const std::array<double, 6>& m = object.state.matrix;
        StageOutline outline{
            .depth = depth,
            .corners = {},
            .anchor = {m[kMoveX] * kPixelsPerUnit, m[kMoveY] * kPixelsPerUnit},
            .linear = {.a = m[kScaleX], .b = m[kSkewB], .c = m[kSkewC], .d = m[kScaleY]}};
        const std::array<Point, 4> corners = Corners(*box);
        for (std::size_t i = 0; i < corners.size(); i++)
            outline.corners.at(i) = Place(object, corners.at(i));
        outlines.push_back(outline);
    }
    return outlines;
}

Linear Reshaped(const Linear& linear, const Reshape& reshape) {
    const double a = reshape.scale_x * linear.a;
    const double b = reshape.scale_x * linear.b;
    const double c = reshape.scale_y * linear.c;
    const double d = reshape.scale_y * linear.d;
    const double cos = std::cos(reshape.turn);
    const double sin = std::sin(reshape.turn);
    return {.a = (a * cos) - (b * sin),
            .b = (a * sin) + (b * cos),
            .c = (c * cos) - (d * sin),
            .d = (c * sin) + (d * cos)};
}

StageOutline ReshapedOutline(const StageOutline& outline, const Reshape& reshape) {
    StageOutline out = outline;
    out.linear = Reshaped(outline.linear, reshape);
    for (Point& corner : out.corners) {
        const std::optional<Point> local = ToLocal(outline, corner);
        if (local) corner = FromLocal(out, *local);
    }
    return out;
}

Reshape ScaleToReach(const StageOutline& outline, Point from, Point to) {
    const std::optional<Point> start = ToLocal(outline, from);
    const std::optional<Point> end = ToLocal(outline, to);
    Reshape reshape;
    if (!start || !end) return reshape;
    if (std::abs((*start)[0]) > kSmallest) reshape.scale_x = (*end)[0] / (*start)[0];
    if (std::abs((*start)[1]) > kSmallest) reshape.scale_y = (*end)[1] / (*start)[1];
    return reshape;
}

Reshape TurnToReach(const StageOutline& outline, Point from, Point to) {
    const double before = std::atan2(from[1] - outline.anchor[1], from[0] - outline.anchor[0]);
    const double after = std::atan2(to[1] - outline.anchor[1], to[0] - outline.anchor[0]);
    return {.scale_x = 1, .scale_y = 1, .turn = after - before};
}

std::optional<uint16_t> DepthAt(const std::vector<StageOutline>& outlines, Point point) {
    for (const StageOutline& outline : std::views::reverse(outlines)) {
        if (Inside(outline, point)) return outline.depth;
    }
    return std::nullopt;
}

}
