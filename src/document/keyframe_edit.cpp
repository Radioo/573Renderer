#include "document/keyframe_edit.h"

#include "document/authored.h"
#include "document/field_values.h"
#include "document/filter_fields.h"
#include "document/filter_values.h"
#include "document/keyframes.h"
#include "document/placement_values.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

namespace {

constexpr std::string_view kFilters = "Filters";

Support::Expected<Track*, std::string> TrackOf(AuthoredDepth& authored, std::string_view property) {
    const auto found = std::ranges::find(authored.tracks, property, &Track::property);
    if (found == authored.tracks.end()) {
        return Support::Unexpected(std::string(property) +
                                   " is not a property this depth animates");
    }
    return &*found;
}

Support::Expected<void, std::string> InRange(const AuthoredDepth& authored, uint32_t frame) {
    if (frame < authored.first_frame || frame > authored.last_frame) {
        return Support::Unexpected("frame " + std::to_string(frame) +
                                   " is outside the frames this depth was owned over");
    }
    return {};
}

template <typename Edit>
Support::Expected<void, std::string> EditKeyFilters(AuthoredDepth& authored, uint32_t frame,
                                                    const Edit& edit) {
    auto track = TrackOf(authored, kFilters);
    if (!track) return Support::Unexpected(track.error());
    const auto key = std::ranges::find((*track)->keys, frame, &Keyframe::frame);
    if (key == (*track)->keys.end())
        return Support::Unexpected("frame " + std::to_string(frame) + " holds no keyframe");
    auto filters = FiltersFrom(key->value);
    if (!filters) return Support::Unexpected(filters.error());
    auto edited = edit(*filters);
    if (!edited) return Support::Unexpected(edited.error());
    return SetKeyframeValue(**track, frame, FilterNumbers(*filters));
}

Ease EaseReaching(const Track& track, uint32_t frame) {
    Ease reaching = track.keys.front().ease;
    for (const Keyframe& key : track.keys) {
        if (key.frame >= frame) break;
        reaching = key.ease;
    }
    return reaching;
}

std::vector<int64_t> ValueAt(const Track& track, uint32_t frame) {
    if (frame < track.keys.front().frame) return track.keys.front().value;
    if (frame > track.keys.back().frame) return track.keys.back().value;
    return SampleTrack(track, frame);
}

}

Support::Expected<void, std::string> AddKeyAt(AuthoredDepth& authored, std::string_view property,
                                              uint32_t frame) {
    auto within = InRange(authored, frame);
    if (!within) return Support::Unexpected(within.error());
    auto track = TrackOf(authored, property);
    if (!track) return Support::Unexpected(track.error());

    const Keyframe key{.frame = frame,
                       .value = ValueAt(**track, frame),
                       .ease = EaseReaching(**track, frame),
                       .bezier = {}};
    return AddKeyframe(**track, key);
}

Support::Expected<void, std::string> RemoveKeyAt(AuthoredDepth& authored, std::string_view property,
                                                 uint32_t frame) {
    auto track = TrackOf(authored, property);
    if (!track) return Support::Unexpected(track.error());
    return RemoveKeyframe(**track, frame);
}

Support::Expected<void, std::string> SetKeyValueAt(AuthoredDepth& authored,
                                                   std::string_view property, uint32_t frame,
                                                   std::string_view value) {
    auto track = TrackOf(authored, property);
    if (!track) return Support::Unexpected(track.error());
    const std::optional<Keyframe> key = KeyAt(authored, property, frame);
    if (!key) return Support::Unexpected("frame " + std::to_string(frame) + " holds no keyframe");
    auto numbers = Numbers(value, key->value.size());
    if (!numbers) return Support::Unexpected(numbers.error());
    return SetKeyValuesAt(authored, property, frame, *numbers);
}

Support::Expected<void, std::string> SetKeyValuesAt(AuthoredDepth& authored,
                                                    std::string_view property, uint32_t frame,
                                                    const std::vector<int64_t>& values) {
    auto track = TrackOf(authored, property);
    if (!track) return Support::Unexpected(track.error());
    return SetKeyframeValue(**track, frame, values);
}

Support::Expected<void, std::string> SetKeyFilterFieldAt(AuthoredDepth& authored, uint32_t frame,
                                                         std::string_view field,
                                                         std::string_view value) {
    return EditKeyFilters(authored, frame, [&](std::vector<AfpAnimation::Filter>& filters) {
        return SetFilterField(filters, field, value);
    });
}

Support::Expected<void, std::string> AddKeyFilterAt(AuthoredDepth& authored, uint32_t frame,
                                                    NewFilter kind) {
    return EditKeyFilters(authored, frame, [&](std::vector<AfpAnimation::Filter>& filters) {
        AddFilter(filters, kind);
        return Support::Expected<void, std::string>();
    });
}

Support::Expected<void, std::string> RemoveKeyFilterAt(AuthoredDepth& authored, uint32_t frame,
                                                       std::string_view field) {
    return EditKeyFilters(authored, frame, [&](std::vector<AfpAnimation::Filter>& filters) {
        return RemoveFilter(filters, field);
    });
}

std::optional<Keyframe> KeyAt(const AuthoredDepth& authored, std::string_view property,
                              uint32_t frame) {
    const auto track = std::ranges::find(authored.tracks, property, &Track::property);
    if (track == authored.tracks.end()) return std::nullopt;
    const auto key = std::ranges::find(track->keys, frame, &Keyframe::frame);
    if (key == track->keys.end()) return std::nullopt;
    return *key;
}

std::string KeyValueText(const Keyframe& key) {
    std::vector<std::string> parts;
    parts.reserve(key.value.size());
    for (const int64_t one : key.value)
        parts.push_back(std::to_string(one));
    return Join(parts);
}

std::optional<Track> GraphedTrack(const AuthoredDepth& authored, std::string_view property) {
    if (PropertyIsStepped(property)) return std::nullopt;
    const auto track = std::ranges::find(authored.tracks, property, &Track::property);
    if (track == authored.tracks.end()) return std::nullopt;
    return *track;
}

}
