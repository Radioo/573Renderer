#include "document/inspector_view.h"

#include "document/anchor_edit.h"
#include "document/authored.h"
#include "document/clip.h"
#include "document/keyframe_edit.h"
#include "document/keyframes.h"
#include "document/placement_edit.h"
#include "document/placement_effect.h"
#include "document/stage_bounds.h"
#include "document/stage_move.h"
#include "document/transform_parts.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr double kUnitsPerPixel = 20.0;
constexpr double kPercent = 100.0;
constexpr double kColourUnit = 255.0;
constexpr uint32_t kThreeD = 0x04000000;
constexpr uint32_t kUseColour = 0x8;
constexpr std::size_t kMoveX = 4;
constexpr std::size_t kMoveY = 5;

constexpr std::string_view kPosition = "Position";
constexpr std::string_view kAnchor = "Anchor";
constexpr std::string_view kScale = "Scale";
constexpr std::string_view kRotation = "Rotation";
constexpr std::string_view kSkew = "Skew";
constexpr std::string_view kMultiply = "Multiply";
constexpr std::string_view kAdd = "Add";

constexpr std::string_view kTranslationTrack = "Translation";
constexpr std::array<std::string_view, 2> kScaleTracks{"Scale", "Short scale"};
constexpr std::array<std::string_view, 4> kMatrixTracks{"Scale", "Short scale", "Rotate skew",
                                                        "Short rotate skew"};
constexpr std::array<std::string_view, 2> kMultiplyTracks{"Multiply colour",
                                                          "Packed multiply colour"};
constexpr std::array<std::string_view, 2> kAddTracks{"Add colour", "Packed add colour"};

bool Tracks(const AuthoredDepth& owned, std::string_view property) {
    return std::ranges::any_of(
        owned.tracks, [property](const Track& track) { return track.property == property; });
}

Keying KeyingOf(const AuthoredDepth* owned, std::initializer_list<std::string_view> properties,
                uint32_t frame) {
    if (owned == nullptr) return Keying::Baked;
    Keying keying = Keying::NotAnimated;
    for (const std::string_view property : properties) {
        if (KeyAt(*owned, property, frame)) return Keying::KeyedHere;
        if (Tracks(*owned, property)) keying = Keying::Animated;
    }
    return keying;
}

std::vector<double> ColourValues(const std::array<double, 4>& colour) {
    std::vector<double> values;
    values.reserve(colour.size());
    for (const double channel : colour)
        values.push_back(std::round(channel * kColourUnit));
    return values;
}

bool WithinBytes(const std::vector<double>& values) {
    return std::ranges::all_of(
        values, [](const double channel) { return channel >= 0 && channel <= kColourUnit; });
}

std::optional<uint32_t> FrameOfTag(const AfpAnimation::Container& clip, std::size_t tag) {
    for (uint32_t frame = 0; frame < clip.frames.size(); frame++) {
        const AfpAnimation::Frame& owner = clip.frames[frame];
        if (tag >= owner.first_tag && tag < owner.first_tag + owner.tag_count) return frame;
    }
    return std::nullopt;
}

struct Placed {
    AppliedState state;
    uint16_t depth = 0;
    uint32_t frame = 0;
    uint32_t matrix_frame = 0;
    const AuthoredDepth* owned = nullptr;
};

void AddAnchorRow(PlacementView& view, const AfpAnimation::Placement& placement,
                  const AfpAnimation::Container& clip, const Placed& placed) {
    if (!placement.origin || placed.owned != nullptr) return;
    const std::optional<std::size_t> tag = LivePlacementTag(clip, placed.depth, placed.frame);
    const std::optional<uint32_t> set_on = tag ? FrameOfTag(clip, *tag) : std::optional<uint32_t>();
    view.transform.push_back(ViewRow{.label = std::string(kAnchor),
                                     .values = {(*placement.origin)[0] / kUnitsPerPixel,
                                                (*placement.origin)[1] / kUnitsPerPixel},
                                     .unit = ViewUnit::Pixels,
                                     .set_on = set_on.value_or(placed.frame),
                                     .keying = Keying::Baked});
}

void AddPlaceRows(PlacementView& view, const AfpAnimation::Placement& placement,
                  const AfpAnimation::Container& clip, const Placed& placed) {
    view.transform.push_back(
        ViewRow{.label = std::string(kPosition),
                .values = {placed.state.matrix[kMoveX] / kUnitsPerPixel,
                           placed.state.matrix[kMoveY] / kUnitsPerPixel},
                .unit = ViewUnit::Pixels,
                .set_on = placed.matrix_frame,
                .keying = KeyingOf(placed.owned, {kTranslationTrack}, placed.frame)});
    AddAnchorRow(view, placement, clip, placed);
    if ((placement.flags & kThreeD) != 0) return;
    const TransformParts parts = PartsOf(LinearOf(placed.state));
    view.transform.push_back(ViewRow{
        .label = std::string(kScale),
        .values = {parts.scale_x * kPercent, parts.scale_y * kPercent},
        .unit = ViewUnit::Percent,
        .set_on = placed.matrix_frame,
        .keying = KeyingOf(placed.owned, {kScaleTracks[0], kScaleTracks[1]}, placed.frame)});
    const Keying turning = KeyingOf(
        placed.owned, {kMatrixTracks[0], kMatrixTracks[1], kMatrixTracks[2], kMatrixTracks[3]},
        placed.frame);
    view.transform.push_back(ViewRow{.label = std::string(kRotation),
                                     .values = {parts.rotation},
                                     .unit = ViewUnit::Degrees,
                                     .set_on = placed.matrix_frame,
                                     .keying = turning});
    view.transform.push_back(ViewRow{.label = std::string(kSkew),
                                     .values = {parts.skew},
                                     .unit = ViewUnit::Degrees,
                                     .set_on = placed.matrix_frame,
                                     .keying = turning});
}

void AddColourRow(PlacementView& view, std::string_view label, std::vector<double> values,
                  uint32_t set_on, Keying keying) {
    const ViewUnit unit = WithinBytes(values) ? ViewUnit::Colour : ViewUnit::Channels;
    view.colours.push_back(ViewRow{.label = std::string(label),
                                   .values = std::move(values),
                                   .unit = unit,
                                   .set_on = set_on,
                                   .keying = keying});
}

const AfpAnimation::Placement* LivePlacement(const AfpAnimation::Container& clip, uint16_t depth,
                                             uint32_t frame) {
    const std::optional<std::size_t> tag = LivePlacementTag(clip, depth, frame);
    if (!tag) return nullptr;
    return std::get_if<AfpAnimation::Placement>(&clip.tags[*tag].body);
}

Support::Expected<TransformParts, std::string> Edited(TransformParts parts, std::string_view label,
                                                      const std::vector<double>& values) {
    if (label == kScale) {
        if (values.size() != 2)
            return Support::Unexpected(std::string("Scale takes a width and a height"));
        parts.scale_x = values[0] / kPercent;
        parts.scale_y = values[1] / kPercent;
        return parts;
    }
    if (values.size() != 1)
        return Support::Unexpected(std::string(label) + " takes one angle in degrees");
    if (label == kRotation) {
        parts.rotation = values[0];
    } else {
        parts.skew = values[0];
    }
    return parts;
}

std::array<int16_t, 4> ColourInts(const std::vector<double>& values) {
    std::array<int16_t, 4> colour{};
    for (std::size_t i = 0; i < colour.size(); i++)
        colour[i] = static_cast<int16_t>(std::lround(values[i]));
    return colour;
}

uint32_t PackedColour(const std::vector<double>& values) {
    uint32_t packed = 0;
    for (const double channel : values)
        packed = (packed << 8U) | (static_cast<uint32_t>(std::lround(channel)) & 0xFFU);
    return packed;
}

std::array<int16_t, 4> FromApplied(const std::array<double, 4>& colour) {
    std::array<int16_t, 4> out{};
    for (std::size_t i = 0; i < out.size(); i++)
        out[i] = static_cast<int16_t>(std::lround(colour[i] * kColourUnit));
    return out;
}

Support::Expected<void, std::string> SetBakedColour(AfpAnimation::Animation& animation, ClipId clip,
                                                    uint16_t depth, uint32_t frame,
                                                    std::string_view label,
                                                    const std::vector<double>& values) {
    if (values.size() != 4)
        return Support::Unexpected(std::string(label) + " takes red, green, blue and alpha");
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
    if ((placement->flags & kUseColour) == 0) {
        const AppliedState& state = shown.back().second;
        if (state.multiply != AppliedState{}.multiply)
            placement->multiply_colour = FromApplied(state.multiply);
        if (state.add != AppliedState{}.add) placement->add_colour = FromApplied(state.add);
        placement->flags |= kUseColour;
    }
    const bool multiply = label == kMultiply;
    std::optional<uint32_t>& packed =
        multiply ? placement->packed_multiply_colour : placement->packed_add_colour;
    std::optional<std::array<int16_t, 4>>& plain =
        multiply ? placement->multiply_colour : placement->add_colour;
    if (packed) {
        packed = PackedColour(values);
        return {};
    }
    plain = ColourInts(values);
    return {};
}

Support::Expected<void, std::string> KeyColour(AuthoredDepth& authored, const BakedDepth& baked,
                                               uint32_t frame, std::string_view label,
                                               const std::vector<double>& values) {
    if (values.size() != 4)
        return Support::Unexpected(std::string(label) + " takes red, green, blue and alpha");
    const bool multiply = label == kMultiply;
    const std::string_view packed = multiply ? kMultiplyTracks[1] : kAddTracks[1];
    const std::string_view plain = multiply ? kMultiplyTracks[0] : kAddTracks[0];
    const bool use_packed = Tracks(authored, packed);
    const std::string_view property = use_packed ? packed : plain;
    if (!use_packed && !Tracks(authored, plain)) {
        auto added = AddTrack(authored, baked, plain);
        if (!added) return Support::Unexpected(added.error());
    }
    if (!KeyAt(authored, property, frame)) {
        auto keyed = AddKeyAt(authored, property, frame);
        if (!keyed) return Support::Unexpected(keyed.error());
    }
    if (use_packed) {
        return SetKeyValuesAt(authored, property, frame,
                              {static_cast<int64_t>(PackedColour(values))});
    }
    std::vector<int64_t> channels;
    channels.reserve(values.size());
    for (const double channel : values)
        channels.push_back(std::lround(channel));
    return SetKeyValuesAt(authored, property, frame, channels);
}

}

std::optional<PlacementView> ViewPlacement(const AfpAnimation::Container& clip, uint16_t depth,
                                           uint32_t frame, const AuthoredDepth* owned) {
    const auto shown = ReplayDepth(clip, depth, frame, frame);
    const AfpAnimation::Placement* placement = LivePlacement(clip, depth, frame);
    if (shown.empty() || placement == nullptr) return std::nullopt;
    const AppliedState& state = shown.back().second;
    const GroupFrames applied = LastApplied(clip, depth, frame);
    const uint32_t matrix_frame = applied.matrix.value_or(frame);
    const uint32_t colour_frame = applied.colour.value_or(frame);

    PlacementView view;
    AddPlaceRows(view, *placement, clip,
                 Placed{.state = state,
                        .depth = depth,
                        .frame = frame,
                        .matrix_frame = matrix_frame,
                        .owned = owned});
    AddColourRow(view, kMultiply, ColourValues(state.multiply), colour_frame,
                 KeyingOf(owned, {kMultiplyTracks[0], kMultiplyTracks[1]}, frame));
    AddColourRow(view, kAdd, ColourValues(state.add), colour_frame,
                 KeyingOf(owned, {kAddTracks[0], kAddTracks[1]}, frame));
    return view;
}

std::optional<std::string> ViewTrack(const AuthoredDepth& owned, std::string_view label) {
    std::vector<std::string_view> candidates;
    if (label == kPosition) {
        candidates = {kTranslationTrack};
    } else if (label == kScale) {
        candidates = {kScaleTracks[0], kScaleTracks[1]};
    } else if (label == kRotation || label == kSkew) {
        candidates = {kMatrixTracks[2], kMatrixTracks[3], kMatrixTracks[0], kMatrixTracks[1]};
    } else if (label == kMultiply) {
        candidates = {kMultiplyTracks[0], kMultiplyTracks[1]};
    } else if (label == kAdd) {
        candidates = {kAddTracks[0], kAddTracks[1]};
    } else {
        return std::nullopt;
    }
    for (const std::string_view property : candidates) {
        if (Tracks(owned, property)) return std::string(property);
    }
    return std::string(candidates.front());
}

Support::Expected<void, std::string> SetViewedBaked(AfpAnimation::Animation& animation, ClipId clip,
                                                    uint16_t depth, uint32_t frame,
                                                    std::string_view label,
                                                    const std::vector<double>& values) {
    const AfpAnimation::Container* target = FindClip(animation, clip);
    if (target == nullptr) return Support::Unexpected(MissingClipMessage(clip));
    const auto shown = ReplayDepth(*target, depth, frame, frame);
    if (shown.empty()) {
        return Support::Unexpected("depth " + std::to_string(depth) + " holds nothing on frame " +
                                   std::to_string(frame));
    }
    const AppliedState& state = shown.back().second;
    if (label == kPosition) {
        if (values.size() != 2)
            return Support::Unexpected(std::string("Position takes an x and a y"));
        return MoveBakedDepth(
            animation, clip, depth, frame,
            StageOffset{.x = values[0] - (state.matrix[kMoveX] / kUnitsPerPixel),
                        .y = values[1] - (state.matrix[kMoveY] / kUnitsPerPixel)});
    }
    if (label == kAnchor) {
        if (values.size() != 2)
            return Support::Unexpected(std::string("Anchor takes an x and a y"));
        const AfpAnimation::Placement* placement = LivePlacement(*target, depth, frame);
        if (placement == nullptr || !placement->origin)
            return Support::Unexpected("depth " + std::to_string(depth) + " has no anchor here");
        const double dx = values[0] - ((*placement->origin)[0] / kUnitsPerPixel);
        const double dy = values[1] - ((*placement->origin)[1] / kUnitsPerPixel);
        const Linear linear = LinearOf(state);
        return MoveAnchor(
            animation, clip, depth, frame,
            Point{(dx * linear.a) + (dy * linear.c), (dx * linear.b) + (dy * linear.d)});
    }
    if (label == kMultiply || label == kAdd)
        return SetBakedColour(animation, clip, depth, frame, label, values);
    if (label != kScale && label != kRotation && label != kSkew)
        return Support::Unexpected(std::string(label) + " is not a value the inspector sets");
    auto parts = Edited(PartsOf(LinearOf(state)), label, values);
    if (!parts) return Support::Unexpected(parts.error());
    return SetBakedLinear(animation, clip, depth, frame, LinearOf(*parts));
}

Support::Expected<void, std::string> SetViewedOwned(AuthoredDepth& authored,
                                                    const BakedDepth& baked, uint32_t frame,
                                                    std::string_view label,
                                                    const std::vector<double>& values) {
    const AppliedState state = KeyedState(authored, baked, frame);
    if (label == kPosition) {
        if (values.size() != 2)
            return Support::Unexpected(std::string("Position takes an x and a y"));
        return MoveOwnedDepth(
            authored, baked, frame,
            StageOffset{.x = values[0] - (state.matrix[kMoveX] / kUnitsPerPixel),
                        .y = values[1] - (state.matrix[kMoveY] / kUnitsPerPixel)});
    }
    if (label == kMultiply || label == kAdd)
        return KeyColour(authored, baked, frame, label, values);
    if (label != kScale && label != kRotation && label != kSkew)
        return Support::Unexpected(std::string(label) + " is not a value the inspector sets");
    auto parts = Edited(PartsOf(LinearOf(state)), label, values);
    if (!parts) return Support::Unexpected(parts.error());
    return SetOwnedLinear(authored, baked, frame, LinearOf(*parts));
}

}
