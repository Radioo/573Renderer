#include "document/stage_move.h"

#include "document/authored.h"
#include "document/clip.h"
#include "document/keyframe_edit.h"
#include "document/keyframes.h"
#include "document/placement_edit.h"
#include "document/placement_effect.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr std::string_view kTranslation = "Translation";
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kThreeD = 0x04000000;
constexpr double kUnitsPerPixel = 20.0;
constexpr double kLongUnit = 1024.0;
constexpr std::size_t kScaleX = 0;
constexpr std::size_t kSkewB = 1;
constexpr std::size_t kSkewC = 2;
constexpr std::size_t kScaleY = 3;
constexpr std::size_t kMoveX = 4;
constexpr std::size_t kMoveY = 5;

int32_t Units(double pixels) {
    return static_cast<int32_t>(std::lround(pixels * kUnitsPerPixel));
}

int32_t Long(double value) {
    return static_cast<int32_t>(std::lround(value * kLongUnit));
}

void CarryMatrix(AfpAnimation::Placement& placement, const AppliedState& state) {
    const std::array<double, 6>& m = state.matrix;
    if (m[kScaleX] != 1.0 || m[kScaleY] != 1.0)
        placement.scale = std::array<int32_t, 2>{Long(m[kScaleX]), Long(m[kScaleY])};
    if (m[kSkewB] != 0.0 || m[kSkewC] != 0.0)
        placement.rotate_skew = std::array<int32_t, 2>{Long(m[kSkewB]), Long(m[kSkewC])};
    placement.translation = std::array<int32_t, 2>{static_cast<int32_t>(std::lround(m[kMoveX])),
                                                   static_cast<int32_t>(std::lround(m[kMoveY]))};
    placement.flags |= kUseMatrix;
}

Support::Expected<void, std::string> Shift(AfpAnimation::Placement& placement, StageOffset offset) {
    std::array<int32_t, 2> moved = placement.translation.value_or(std::array<int32_t, 2>{0, 0});
    const int64_t x = static_cast<int64_t>(moved[0]) + Units(offset.x);
    const int64_t y = static_cast<int64_t>(moved[1]) + Units(offset.y);
    const auto fits = [](int64_t value) {
        return value >= std::numeric_limits<int32_t>::min() &&
               value <= std::numeric_limits<int32_t>::max();
    };
    if (!fits(x) || !fits(y))
        return Support::Unexpected(std::string("the move goes off the stage"));
    moved = {static_cast<int32_t>(x), static_cast<int32_t>(y)};
    placement.translation = moved;
    return {};
}

}

Support::Expected<void, std::string> MoveBakedDepth(AfpAnimation::Animation& animation, ClipId clip,
                                                    uint16_t depth, uint32_t frame,
                                                    StageOffset offset) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    AfpAnimation::Container& target = **found;
    const std::optional<std::size_t> tag = LivePlacementTag(target, depth, frame);
    if (!tag) {
        return Support::Unexpected("depth " + std::to_string(depth) + " holds nothing on frame " +
                                   std::to_string(frame));
    }
    const auto shown = ReplayDepth(target, depth, frame, frame);
    auto* placement = std::get_if<AfpAnimation::Placement>(&target.tags[*tag].body);
    if (placement == nullptr || shown.empty())
        return Support::Unexpected("depth " + std::to_string(depth) + " is not placed here");
    if ((placement->flags & kThreeD) != 0)
        return Support::Unexpected("depth " + std::to_string(depth) + " is placed in 3D");
    if ((placement->flags & kUseMatrix) == 0) CarryMatrix(*placement, shown.back().second);
    return Shift(*placement, offset);
}

Support::Expected<void, std::string> MoveOwnedDepth(AuthoredDepth& authored,
                                                    const BakedDepth& baked, uint32_t frame,
                                                    StageOffset offset) {
    if ((baked.create.flags & kThreeD) != 0)
        return Support::Unexpected("depth " + std::to_string(authored.depth) + " is placed in 3D");
    AuthoredDepth edited = authored;
    const bool tracked = std::ranges::any_of(
        edited.tracks, [](const Track& track) { return track.property == kTranslation; });
    if (!tracked) {
        auto added = AddTrack(edited, baked, kTranslation);
        if (!added) return Support::Unexpected(added.error());
    }
    if (!KeyAt(edited, kTranslation, frame)) {
        auto keyed = AddKeyAt(edited, kTranslation, frame);
        if (!keyed) return Support::Unexpected(keyed.error());
    }
    const auto track = std::ranges::find(edited.tracks, kTranslation, &Track::property);
    const std::optional<Keyframe> key = KeyAt(edited, kTranslation, frame);
    if (track == edited.tracks.end() || !key || key->value.size() != 2)
        return Support::Unexpected(std::string("the translation track has no key to move"));
    AfpAnimation::Placement moved;
    moved.translation = std::array<int32_t, 2>{static_cast<int32_t>(key->value[0]),
                                               static_cast<int32_t>(key->value[1])};
    auto shifted = Shift(moved, offset);
    if (!shifted) return Support::Unexpected(shifted.error());
    const std::vector<int64_t> value{(*moved.translation)[0], (*moved.translation)[1]};
    auto set = SetKeyframeValue(*track, frame, value);
    if (!set) return Support::Unexpected(set.error());
    authored = std::move(edited);
    return {};
}

}
