#include "document/anchor_edit.h"

#include "document/clip.h"
#include "document/placement_effect.h"
#include "document/span_edit.h"
#include "document/span_split.h"
#include "document/span_tags.h"
#include "document/stage_bounds.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace Document {

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kThreeD = 0x04000000;
constexpr double kUnitsPerPixel = 20.0;

using Units = std::array<int32_t, 2>;

std::string Named(uint16_t depth) {
    return "depth " + std::to_string(depth);
}

Units OriginOn(const AfpAnimation::Container& clip, uint16_t depth, const Span& span,
               uint32_t frame) {
    Units origin{0, 0};
    for (uint32_t at = span.first_frame; at <= frame && at < clip.frames.size(); at++) {
        const AfpAnimation::Frame& owner = clip.frames[at];
        for (std::size_t tag = 0; tag < owner.tag_count; tag++) {
            const std::size_t index = owner.first_tag + tag;
            if (index >= clip.tags.size()) break;
            const auto* placement = std::get_if<AfpAnimation::Placement>(&clip.tags[index].body);
            if (placement != nullptr && placement->depth == depth && placement->origin)
                origin = *placement->origin;
        }
    }
    return origin;
}

Support::Expected<void, std::string> ShiftPlacement(AfpAnimation::Placement& placement,
                                                    const Units& shift) {
    if ((placement.flags & kThreeD) != 0) {
        return Support::Unexpected(Named(placement.depth) +
                                   " is placed in 3D, so its anchor has no place on the stage");
    }
    if (placement.geometry) {
        return Support::Unexpected(Named(placement.depth) +
                                   " carries geometry, which the game draws without an origin");
    }
    const bool creates = (placement.flags & kUpdateExisting) == 0;
    if (placement.origin) {
        (*placement.origin)[0] += shift[0];
        (*placement.origin)[1] += shift[1];
    } else if (creates) {
        placement.origin = shift;
    }
    if ((placement.flags & kUseMatrix) == 0) {
        if (!creates) return {};
        placement.flags |= kUseMatrix;
    }
    AppliedState own;
    ApplyPlacement(own, placement);
    const std::array<double, 6>& m = own.matrix;
    Units moved = placement.translation.value_or(Units{0, 0});
    moved[0] += static_cast<int32_t>(std::lround((m[0] * shift[0]) + (m[2] * shift[1])));
    moved[1] += static_cast<int32_t>(std::lround((m[1] * shift[0]) + (m[3] * shift[1])));
    placement.translation = moved;
    return {};
}

Support::Expected<void, std::string> ShiftAnchor(AfpAnimation::Animation& animation, ClipId clip,
                                                 uint16_t depth, const Span& span,
                                                 const Units& shift) {
    AfpAnimation::Animation edited = animation;
    AfpAnimation::Container& target = **RequireClip(edited, clip);
    for (const std::size_t index : SpanTags(target, depth, span)) {
        auto* placement = std::get_if<AfpAnimation::Placement>(&target.tags[index].body);
        if (placement == nullptr) continue;
        auto shifted = ShiftPlacement(*placement, shift);
        if (!shifted) return Support::Unexpected(shifted.error());
    }
    animation = std::move(edited);
    return {};
}

}

Support::Expected<void, std::string> CentreAnchor(AfpAnimation::Animation& animation, ClipId clip,
                                                  uint16_t depth, uint32_t frame,
                                                  const std::map<uint16_t, Box>& shape_bounds) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    const AfpAnimation::Container& target = **found;
    const std::optional<Span> span = SpanOfDepth(target, depth, frame);
    if (!span) {
        return Support::Unexpected(Named(depth) + " holds nothing on frame " +
                                   std::to_string(frame));
    }
    const std::optional<uint16_t> character = CharacterOn(target, depth, *span, frame);
    const std::optional<Box> box =
        character ? CharacterBox(animation, *character, shape_bounds) : std::nullopt;
    if (!box) {
        return Support::Unexpected(Named(depth) + " shows nothing with a box the editor knows, " +
                                   "so it has no centre to put the anchor on");
    }
    const Units origin = OriginOn(target, depth, *span, frame);
    const Units centre{
        static_cast<int32_t>(std::lround((box->left + box->right) / 2 * kUnitsPerPixel)),
        static_cast<int32_t>(std::lround((box->top + box->bottom) / 2 * kUnitsPerPixel))};
    const Units shift{centre[0] - origin[0], centre[1] - origin[1]};
    if (shift == Units{0, 0}) {
        return Support::Unexpected("the anchor of " + Named(depth) +
                                   " is already at the centre of what it shows");
    }
    return ShiftAnchor(animation, clip, depth, *span, shift);
}

}
