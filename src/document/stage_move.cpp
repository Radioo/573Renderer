#include "document/stage_move.h"

#include "document/authored.h"
#include "document/clip.h"
#include "document/frame_edit.h"
#include "document/keyframe_edit.h"
#include "document/keyframes.h"
#include "document/placement_edit.h"
#include "document/placement_effect.h"
#include "document/stage_bounds.h"
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
constexpr std::string_view kScale = "Scale";
constexpr std::string_view kShortScale = "Short scale";
constexpr std::string_view kRotateSkew = "Rotate skew";
constexpr std::string_view kShortRotateSkew = "Short rotate skew";
constexpr double kShortUnit = 32768.0;
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

std::optional<int32_t> Encoded(double value, double unit, int64_t lowest, int64_t highest) {
    const double scaled = std::round(value * unit);
    if (scaled < static_cast<double>(lowest) || scaled > static_cast<double>(highest))
        return std::nullopt;
    return static_cast<int32_t>(scaled);
}

std::optional<std::array<int32_t, 2>> LongPair(double first, double second) {
    const auto x = Encoded(first, kLongUnit, std::numeric_limits<int32_t>::min(),
                           std::numeric_limits<int32_t>::max());
    const auto y = Encoded(second, kLongUnit, std::numeric_limits<int32_t>::min(),
                           std::numeric_limits<int32_t>::max());
    if (!x || !y) return std::nullopt;
    return std::array<int32_t, 2>{*x, *y};
}

std::optional<std::array<int16_t, 2>> ShortPair(double first, double second) {
    const auto x = Encoded(first, kShortUnit, std::numeric_limits<int16_t>::min(),
                           std::numeric_limits<int16_t>::max());
    const auto y = Encoded(second, kShortUnit, std::numeric_limits<int16_t>::min(),
                           std::numeric_limits<int16_t>::max());
    if (!x || !y) return std::nullopt;
    return std::array<int16_t, 2>{static_cast<int16_t>(*x), static_cast<int16_t>(*y)};
}

Linear LinearOf(const AppliedState& state) {
    const std::array<double, 6>& m = state.matrix;
    return {.a = m[kScaleX], .b = m[kSkewB], .c = m[kSkewC], .d = m[kScaleY]};
}

Support::Expected<void, std::string> WritePart(std::optional<std::array<int32_t, 2>>& long_form,
                                               std::optional<std::array<int16_t, 2>>& short_form,
                                               double first, double second, bool identity) {
    if (short_form) {
        const auto packed = ShortPair(first, second);
        if (packed) {
            short_form = packed;
            return {};
        }
        short_form.reset();
    }
    if (identity) {
        long_form.reset();
        return {};
    }
    const auto packed = LongPair(first, second);
    if (!packed) return Support::Unexpected(std::string("the new matrix does not fit a placement"));
    long_form = packed;
    return {};
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

bool Tracks(const AuthoredDepth& authored, std::string_view property) {
    return std::ranges::any_of(
        authored.tracks, [property](const Track& track) { return track.property == property; });
}

Support::Expected<Track*, std::string> KeyedTrack(AuthoredDepth& authored, const BakedDepth& baked,
                                                  std::string_view property, uint32_t frame) {
    if (!Tracks(authored, property)) {
        auto added = AddTrack(authored, baked, property);
        if (!added) return Support::Unexpected(added.error());
    }
    if (!KeyAt(authored, property, frame)) {
        auto keyed = AddKeyAt(authored, property, frame);
        if (!keyed) return Support::Unexpected(keyed.error());
    }
    const auto track = std::ranges::find(authored.tracks, property, &Track::property);
    if (track == authored.tracks.end())
        return Support::Unexpected(std::string(property) + " has no track to key");
    return &*track;
}

Support::Expected<void, std::string> KeyValue(AuthoredDepth& authored, const BakedDepth& baked,
                                              std::string_view property, uint32_t frame,
                                              const std::vector<int64_t>& value) {
    auto track = KeyedTrack(authored, baked, property, frame);
    if (!track) return Support::Unexpected(track.error());
    return SetKeyframeValue(**track, frame, value);
}

Support::Expected<void, std::string> KeyPart(AuthoredDepth& authored, const BakedDepth& baked,
                                             uint32_t frame, std::string_view long_name,
                                             std::string_view short_name,
                                             std::array<double, 2> part) {
    if (Tracks(authored, short_name)) {
        const auto packed = ShortPair(part[0], part[1]);
        if (!packed)
            return Support::Unexpected(std::string(short_name) + " cannot hold the new matrix");
        return KeyValue(authored, baked, short_name, frame, {(*packed)[0], (*packed)[1]});
    }
    const auto packed = LongPair(part[0], part[1]);
    if (!packed) return Support::Unexpected(std::string("the new matrix does not fit a placement"));
    return KeyValue(authored, baked, long_name, frame, {(*packed)[0], (*packed)[1]});
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

Support::Expected<void, std::string> PlaceAtPoint(AfpAnimation::Animation& animation, ClipId clip,
                                                  uint16_t depth, uint16_t character,
                                                  uint32_t first_frame, uint32_t last_frame,
                                                  StageOffset point) {
    AfpAnimation::Animation edited = animation;
    auto added = AddDepth(edited, clip, depth, character, first_frame, last_frame);
    if (!added) return Support::Unexpected(added.error());
    auto moved = MoveBakedDepth(edited, clip, depth, first_frame, point);
    if (!moved) return Support::Unexpected(moved.error());
    animation = std::move(edited);
    return {};
}

Support::Expected<void, std::string> ReshapeBakedDepth(AfpAnimation::Animation& animation,
                                                       ClipId clip, uint16_t depth, uint32_t frame,
                                                       const Reshape& reshape) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    AfpAnimation::Container& target = **found;
    const std::optional<std::size_t> tag = LivePlacementTag(target, depth, frame);
    const auto shown = ReplayDepth(target, depth, frame, frame);
    auto* placement = tag ? std::get_if<AfpAnimation::Placement>(&target.tags[*tag].body) : nullptr;
    if (placement == nullptr || shown.empty()) {
        return Support::Unexpected("depth " + std::to_string(depth) + " holds nothing on frame " +
                                   std::to_string(frame));
    }
    if ((placement->flags & kThreeD) != 0)
        return Support::Unexpected("depth " + std::to_string(depth) + " is placed in 3D");
    if ((placement->flags & kUseMatrix) == 0) CarryMatrix(*placement, shown.back().second);
    const Linear next = Reshaped(LinearOf(shown.back().second), reshape);
    auto scaled = WritePart(placement->scale, placement->short_scale, next.a, next.d,
                            next.a == 1.0 && next.d == 1.0);
    if (!scaled) return Support::Unexpected(scaled.error());
    return WritePart(placement->rotate_skew, placement->short_rotate_skew, next.b, next.c,
                     next.b == 0.0 && next.c == 0.0);
}

Support::Expected<void, std::string> ReshapeOwnedDepth(AuthoredDepth& authored,
                                                       const BakedDepth& baked, uint32_t frame,
                                                       const Reshape& reshape) {
    if ((baked.create.flags & kThreeD) != 0)
        return Support::Unexpected("depth " + std::to_string(authored.depth) + " is placed in 3D");
    const Linear next = Reshaped(LinearOf(KeyedState(authored, baked, frame)), reshape);
    AuthoredDepth edited = authored;
    auto scaled = KeyPart(edited, baked, frame, kScale, kShortScale, {next.a, next.d});
    if (!scaled) return Support::Unexpected(scaled.error());
    auto turned = KeyPart(edited, baked, frame, kRotateSkew, kShortRotateSkew, {next.b, next.c});
    if (!turned) return Support::Unexpected(turned.error());
    authored = std::move(edited);
    return {};
}

Support::Expected<void, std::string> MoveOwnedDepth(AuthoredDepth& authored,
                                                    const BakedDepth& baked, uint32_t frame,
                                                    StageOffset offset) {
    if ((baked.create.flags & kThreeD) != 0)
        return Support::Unexpected("depth " + std::to_string(authored.depth) + " is placed in 3D");
    AuthoredDepth edited = authored;
    auto track = KeyedTrack(edited, baked, kTranslation, frame);
    if (!track) return Support::Unexpected(track.error());
    const std::optional<Keyframe> key = KeyAt(edited, kTranslation, frame);
    if (!key || key->value.size() != 2)
        return Support::Unexpected(std::string("the translation track has no key to move"));
    AfpAnimation::Placement moved;
    moved.translation = std::array<int32_t, 2>{static_cast<int32_t>(key->value[0]),
                                               static_cast<int32_t>(key->value[1])};
    auto shifted = Shift(moved, offset);
    if (!shifted) return Support::Unexpected(shifted.error());
    const std::vector<int64_t> value{(*moved.translation)[0], (*moved.translation)[1]};
    auto set = SetKeyframeValue(**track, frame, value);
    if (!set) return Support::Unexpected(set.error());
    authored = std::move(edited);
    return {};
}

Support::Expected<void, std::string> SketchOwnedDepth(AuthoredDepth& authored,
                                                      const BakedDepth& baked, uint32_t pressed,
                                                      const SketchedOffsets& offsets) {
    if ((baked.create.flags & kThreeD) != 0)
        return Support::Unexpected("depth " + std::to_string(authored.depth) + " is placed in 3D");
    if (offsets.empty()) return Support::Unexpected(std::string("nothing was sketched"));
    for (const auto& [frame, offset] : offsets) {
        if (frame < authored.first_frame || frame > authored.last_frame) {
            return Support::Unexpected("frame " + std::to_string(frame) +
                                       " is outside the frames this depth was owned over");
        }
    }
    AuthoredDepth edited = authored;
    auto track = KeyedTrack(edited, baked, kTranslation, pressed);
    if (!track) return Support::Unexpected(track.error());
    const std::optional<Keyframe> base = KeyAt(edited, kTranslation, pressed);
    if (!base || base->value.size() != 2)
        return Support::Unexpected(std::string("the translation track has no key to sketch from"));
    const uint32_t first = offsets.begin()->first;
    const uint32_t last = offsets.rbegin()->first;
    std::erase_if((*track)->keys, [first, last](const Keyframe& key) {
        return key.frame >= first && key.frame <= last;
    });
    for (const auto& [frame, offset] : offsets) {
        AfpAnimation::Placement moved;
        moved.translation = std::array<int32_t, 2>{static_cast<int32_t>(base->value[0]),
                                                   static_cast<int32_t>(base->value[1])};
        auto shifted = Shift(moved, offset);
        if (!shifted) return Support::Unexpected(shifted.error());
        auto added = AddKeyframe(
            **track, Keyframe{.frame = frame,
                              .value = {(*moved.translation)[0], (*moved.translation)[1]},
                              .ease = Ease::Linear,
                              .bezier = {}});
        if (!added) return Support::Unexpected(added.error());
    }
    authored = std::move(edited);
    return {};
}

}
