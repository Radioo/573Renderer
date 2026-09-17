#include "document/key_selection.h"

#include "document/authored.h"
#include "document/keyframes.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Document {

namespace {

Track* TrackNamed(AuthoredDepth& authored, std::string_view property) {
    const auto found = std::ranges::find(authored.tracks, property, &Track::property);
    return found == authored.tracks.end() ? nullptr : &*found;
}

const Track* TrackNamed(const AuthoredDepth& authored, std::string_view property) {
    const auto found = std::ranges::find(authored.tracks, property, &Track::property);
    return found == authored.tracks.end() ? nullptr : &*found;
}

Support::Expected<uint32_t, std::string> FrameWithin(const AuthoredDepth& authored, int64_t frame) {
    if (std::cmp_less(frame, authored.first_frame) ||
        std::cmp_greater(frame, authored.last_frame)) {
        return Support::Unexpected("frame " + std::to_string(frame) +
                                   " is outside the frames this depth was owned over");
    }
    return static_cast<uint32_t>(frame);
}

bool Holds(const Track& track, uint32_t frame) {
    return std::ranges::find(track.keys, frame, &Keyframe::frame) != track.keys.end();
}

bool Selected(const std::vector<KeyRef>& keys, const KeyRef& key) {
    return std::ranges::find(keys, key) != keys.end();
}

std::string Missing(const KeyRef& key) {
    return "there is no " + key.property + " keyframe at frame " + std::to_string(key.frame);
}

Support::Expected<void, std::string> PasteTrack(AuthoredDepth& authored, const BakedDepth& baked,
                                                const Track& copied, uint32_t frame,
                                                std::vector<KeyRef>& pasted) {
    if (TrackNamed(authored, copied.property) == nullptr) {
        auto added = AddTrack(authored, baked, copied.property);
        if (!added) return Support::Unexpected(added.error());
    }
    Track* track = TrackNamed(authored, copied.property);
    if (track == nullptr)
        return Support::Unexpected(copied.property + " is not a property this depth animates");
    for (const Keyframe& key : copied.keys) {
        auto at = FrameWithin(authored, static_cast<int64_t>(frame) + key.frame);
        if (!at) return Support::Unexpected(at.error());
        Keyframe placed = key;
        placed.frame = *at;
        const auto existing = std::ranges::find(track->keys, *at, &Keyframe::frame);
        if (existing != track->keys.end()) {
            if (existing->value.size() != placed.value.size()) {
                return Support::Unexpected("the track for " + track->property + " keys " +
                                           std::to_string(existing->value.size()) +
                                           " values, not " + std::to_string(placed.value.size()));
            }
            *existing = placed;
        } else {
            auto added = AddKeyframe(*track, placed);
            if (!added) return Support::Unexpected(added.error());
        }
        pasted.push_back(KeyRef{.property = track->property, .frame = *at});
    }
    return CheckTrack(*track);
}

Support::Expected<void, std::string> ShiftTrack(const AuthoredDepth& limits, Track& track,
                                                const std::set<uint32_t>& selected, int64_t by,
                                                std::vector<KeyRef>& shifted) {
    for (Keyframe& key : track.keys) {
        if (!selected.contains(key.frame)) continue;
        auto moved = FrameWithin(limits, static_cast<int64_t>(key.frame) + by);
        if (!moved) return Support::Unexpected(moved.error());
        key.frame = *moved;
        shifted.push_back(KeyRef{.property = track.property, .frame = *moved});
    }
    std::ranges::stable_sort(track.keys, {}, &Keyframe::frame);
    for (std::size_t i = 1; i < track.keys.size(); i++) {
        if (track.keys[i].frame == track.keys[i - 1].frame) {
            return Support::Unexpected("frame " + std::to_string(track.keys[i].frame) +
                                       " already holds a " + track.property + " keyframe");
        }
    }
    return {};
}

}

std::vector<KeyRef> AllKeys(const AuthoredDepth& authored) {
    std::vector<KeyRef> keys;
    for (const Track& track : authored.tracks) {
        for (const Keyframe& key : track.keys)
            keys.push_back(KeyRef{.property = track.property, .frame = key.frame});
    }
    return keys;
}

Support::Expected<KeyClip, std::string> CopyKeys(const AuthoredDepth& authored,
                                                 const std::vector<KeyRef>& keys) {
    if (keys.empty()) return Support::Unexpected(std::string("no keyframes are selected"));
    const uint32_t earliest = std::ranges::min(keys, {}, &KeyRef::frame).frame;
    KeyClip clip;
    for (const Track& track : authored.tracks) {
        Track copied{.property = track.property, .keys = {}};
        for (const Keyframe& key : track.keys) {
            if (!Selected(keys, KeyRef{.property = track.property, .frame = key.frame})) continue;
            Keyframe kept = key;
            kept.frame = key.frame - earliest;
            copied.keys.push_back(std::move(kept));
        }
        if (!copied.keys.empty()) clip.tracks.push_back(std::move(copied));
    }
    for (const KeyRef& key : keys) {
        const Track* track = TrackNamed(authored, key.property);
        if (track == nullptr || !Holds(*track, key.frame)) return Support::Unexpected(Missing(key));
    }
    return clip;
}

Support::Expected<std::vector<KeyRef>, std::string>
PasteKeys(AuthoredDepth& authored, const BakedDepth& baked, const KeyClip& clip, uint32_t frame) {
    if (clip.tracks.empty()) return Support::Unexpected(std::string("nothing is copied"));
    AuthoredDepth edited = authored;
    std::vector<KeyRef> pasted;
    for (const Track& copied : clip.tracks) {
        auto done = PasteTrack(edited, baked, copied, frame, pasted);
        if (!done) return Support::Unexpected(done.error());
    }
    authored = std::move(edited);
    return pasted;
}

Support::Expected<void, std::string> RemoveKeys(AuthoredDepth& authored,
                                                const std::vector<KeyRef>& keys) {
    if (keys.empty()) return Support::Unexpected(std::string("no keyframes are selected"));
    AuthoredDepth edited = authored;
    for (const KeyRef& key : keys) {
        Track* track = TrackNamed(edited, key.property);
        if (track == nullptr) return Support::Unexpected(Missing(key));
        auto removed = RemoveKeyframe(*track, key.frame);
        if (!removed) return Support::Unexpected(removed.error());
    }
    authored = std::move(edited);
    return {};
}

Support::Expected<void, std::string>
SetKeysEase(AuthoredDepth& authored, const std::vector<KeyRef>& keys, Ease ease, Bezier bezier) {
    if (keys.empty()) return Support::Unexpected(std::string("no keyframes are selected"));
    AuthoredDepth edited = authored;
    for (const KeyRef& key : keys) {
        Track* track = TrackNamed(edited, key.property);
        if (track == nullptr) return Support::Unexpected(Missing(key));
        auto eased = SetKeyframeEase(*track, key.frame, ease, bezier);
        if (!eased) return Support::Unexpected(eased.error());
    }
    authored = std::move(edited);
    return {};
}

Support::Expected<std::vector<KeyRef>, std::string>
ShiftKeys(AuthoredDepth& authored, const std::vector<KeyRef>& keys, int64_t by) {
    if (keys.empty()) return Support::Unexpected(std::string("no keyframes are selected"));
    AuthoredDepth edited = authored;
    std::vector<KeyRef> shifted;
    for (Track& track : edited.tracks) {
        std::set<uint32_t> selected;
        for (const KeyRef& key : keys) {
            if (key.property == track.property) selected.insert(key.frame);
        }
        if (selected.empty()) continue;
        for (const uint32_t frame : selected) {
            if (!Holds(track, frame)) {
                return Support::Unexpected(
                    Missing(KeyRef{.property = track.property, .frame = frame}));
            }
        }
        auto moved = ShiftTrack(authored, track, selected, by, shifted);
        if (!moved) return Support::Unexpected(moved.error());
    }
    if (shifted.size() != std::set<KeyRef>(keys.begin(), keys.end()).size())
        return Support::Unexpected(std::string("a selected keyframe belongs to no track"));
    authored = std::move(edited);
    return shifted;
}

}
