#include "document/key_selection.h"

#include "document/authored.h"
#include "document/keyframes.h"
#include "document/placement_values.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
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
            if (!PropertyIsStepped(track->property) &&
                existing->value.size() != placed.value.size()) {
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

Support::Expected<void, std::string> CheckHeld(const AuthoredDepth& authored,
                                               const std::vector<KeyRef>& keys) {
    for (const KeyRef& key : keys) {
        const Track* track = TrackNamed(authored, key.property);
        if (track == nullptr || !Holds(*track, key.frame)) return Support::Unexpected(Missing(key));
    }
    return {};
}

Support::Expected<void, std::string> StretchTrack(const AuthoredDepth& authored, Track& track,
                                                  const std::vector<KeyRef>& keys,
                                                  const KeyStretch& stretch,
                                                  std::map<KeyRef, uint32_t>& moved) {
    std::vector<uint32_t> frames;
    frames.reserve(track.keys.size());
    for (const Keyframe& key : track.keys) {
        const KeyRef ref{.property = track.property, .frame = key.frame};
        if (!Selected(keys, ref)) {
            frames.push_back(key.frame);
            continue;
        }
        auto at = FrameWithin(authored, StretchedFrame(stretch, key.frame));
        if (!at) return Support::Unexpected(at.error());
        moved[ref] = *at;
        frames.push_back(*at);
    }
    for (std::size_t i = 1; i < frames.size(); i++) {
        if (frames[i] <= frames[i - 1]) {
            return Support::Unexpected(
                "stretching would put the " + track.property + " keyframe from frame " +
                std::to_string(track.keys[i].frame) + " on or before the one from frame " +
                std::to_string(track.keys[i - 1].frame));
        }
    }
    for (std::size_t i = 0; i < frames.size(); i++)
        track.keys[i].frame = frames[i];
    return {};
}

constexpr double kEasyInfluence = 1.0 / 3.0;
constexpr double kEasyEnd = 2.0 / 3.0;

Bezier Leaving(const Keyframe& key) {
    if (key.ease == Ease::Bezier) return key.bezier;
    return Bezier{.x1 = kEasyInfluence, .y1 = kEasyInfluence, .x2 = kEasyEnd, .y2 = kEasyEnd};
}

bool EaseSegment(Track& track, std::size_t from, bool slow_start, bool slow_end) {
    if (from + 1 >= track.keys.size()) return false;
    Keyframe& key = track.keys[from];
    Bezier curve = Leaving(key);
    if (slow_start) {
        curve.x1 = kEasyInfluence;
        curve.y1 = 0.0;
    }
    if (slow_end) {
        curve.x2 = kEasyEnd;
        curve.y2 = 1.0;
    }
    key.ease = Ease::Bezier;
    key.bezier = curve;
    return true;
}

Bezier Mirrored(const Bezier& curve) {
    return Bezier{
        .x1 = 1.0 - curve.x2, .y1 = 1.0 - curve.y2, .x2 = 1.0 - curve.x1, .y2 = 1.0 - curve.y1};
}

Support::Expected<std::map<uint32_t, uint32_t>, std::string>
ReverseTrack(Track& track, const std::set<uint32_t>& selected) {
    const uint32_t first = *selected.begin();
    const uint32_t last = *selected.rbegin();
    std::vector<Keyframe> chosen;
    std::vector<Keyframe> kept;
    for (const Keyframe& key : track.keys) {
        if (selected.contains(key.frame)) {
            chosen.push_back(key);
            continue;
        }
        if (key.frame > first && key.frame < last) {
            return Support::Unexpected("the " + track.property + " keyframe at frame " +
                                       std::to_string(key.frame) +
                                       " lies between the selected ones, so select it too");
        }
        kept.push_back(key);
    }
    std::map<uint32_t, uint32_t> moved;
    for (std::size_t i = 0; i < chosen.size(); i++) {
        Keyframe key = chosen[i];
        key.frame = first + last - chosen[i].frame;
        const Keyframe& leaving = i == 0 ? chosen.back() : chosen[i - 1];
        key.ease = leaving.ease;
        key.bezier = i == 0 ? leaving.bezier : Mirrored(leaving.bezier);
        moved[chosen[i].frame] = key.frame;
        kept.push_back(std::move(key));
    }
    std::ranges::sort(kept, {}, &Keyframe::frame);
    track.keys = std::move(kept);
    return moved;
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
    auto held = CheckHeld(authored, keys);
    if (!held) return Support::Unexpected(held.error());
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
        if (ease != Ease::Hold && PropertyIsStepped(track->property)) {
            return Support::Unexpected(track->property +
                                       " jumps from one keyframe to the next and only holds");
        }
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

Support::Expected<std::vector<KeyRef>, std::string> ReverseKeys(AuthoredDepth& authored,
                                                                const std::vector<KeyRef>& keys) {
    if (keys.empty()) return Support::Unexpected(std::string("no keyframes are selected"));
    AuthoredDepth edited = authored;
    std::map<KeyRef, uint32_t> moved;
    auto held = CheckHeld(edited, keys);
    if (!held) return Support::Unexpected(held.error());
    for (Track& track : edited.tracks) {
        std::set<uint32_t> selected;
        for (const KeyRef& key : keys) {
            if (key.property == track.property) selected.insert(key.frame);
        }
        if (selected.size() < 2) continue;
        auto flipped = ReverseTrack(track, selected);
        if (!flipped) return Support::Unexpected(flipped.error());
        for (const auto& [from, to] : *flipped)
            moved[KeyRef{.property = track.property, .frame = from}] = to;
    }
    if (moved.empty()) {
        return Support::Unexpected(
            std::string("time-reversing needs two selected keyframes of one property"));
    }
    std::vector<KeyRef> reversed;
    reversed.reserve(keys.size());
    for (const KeyRef& key : keys) {
        const auto found = moved.find(key);
        reversed.push_back(
            found == moved.end() ? key : KeyRef{.property = key.property, .frame = found->second});
    }
    authored = std::move(edited);
    return reversed;
}

Support::Expected<std::vector<KeyRef>, std::string>
StretchKeys(AuthoredDepth& authored, const std::vector<KeyRef>& keys, const KeyStretch& stretch) {
    if (keys.empty()) return Support::Unexpected(std::string("no keyframes are selected"));
    if (stretch.scale == 0 || stretch.over == 0 || (stretch.scale < 0) != (stretch.over < 0)) {
        return Support::Unexpected("keyframes can only be stretched away from or towards frame " +
                                   std::to_string(stretch.anchor) + ", not onto or past it");
    }
    auto held = CheckHeld(authored, keys);
    if (!held) return Support::Unexpected(held.error());
    AuthoredDepth edited = authored;
    std::map<KeyRef, uint32_t> moved;
    for (Track& track : edited.tracks) {
        auto stretched = StretchTrack(authored, track, keys, stretch, moved);
        if (!stretched) return Support::Unexpected(stretched.error());
    }
    if (std::ranges::all_of(moved,
                            [](const auto& entry) { return entry.first.frame == entry.second; }))
        return Support::Unexpected(std::string("the stretch moves no keyframe"));
    std::vector<KeyRef> placed;
    placed.reserve(keys.size());
    for (const KeyRef& key : keys)
        placed.push_back(KeyRef{.property = key.property, .frame = moved.at(key)});
    authored = std::move(edited);
    return placed;
}

int64_t StretchedFrame(const KeyStretch& stretch, uint32_t frame) {
    const int64_t over = stretch.over < 0 ? -stretch.over : stretch.over;
    const int64_t scale = stretch.over < 0 ? -stretch.scale : stretch.scale;
    const int64_t twice = (2 * (static_cast<int64_t>(frame) - stretch.anchor) * scale) + over;
    const int64_t whole = twice / (2 * over);
    const bool below = twice % (2 * over) != 0 && twice < 0;
    return static_cast<int64_t>(stretch.anchor) + whole - (below ? 1 : 0);
}

Support::Expected<void, std::string> EasyEaseKeys(AuthoredDepth& authored,
                                                  const std::vector<KeyRef>& keys, EasySide side) {
    if (keys.empty()) return Support::Unexpected(std::string("no keyframes are selected"));
    auto held = CheckHeld(authored, keys);
    if (!held) return Support::Unexpected(held.error());
    AuthoredDepth edited = authored;
    bool eased = false;
    for (const KeyRef& ref : keys) {
        Track& track = *TrackNamed(edited, ref.property);
        if (PropertyIsStepped(track.property)) {
            return Support::Unexpected(track.property +
                                       " jumps from one keyframe to the next and only holds");
        }
        const auto at = static_cast<std::size_t>(
            std::ranges::find(track.keys, ref.frame, &Keyframe::frame) - track.keys.begin());
        if (side != EasySide::In) eased = EaseSegment(track, at, true, false) || eased;
        if (side != EasySide::Out && at > 0)
            eased = EaseSegment(track, at - 1, false, true) || eased;
    }
    if (!eased) {
        return Support::Unexpected(
            std::string("no selected keyframe has a neighbour on that side to ease towards"));
    }
    authored = std::move(edited);
    return {};
}

Support::Expected<void, std::string> ToggleHoldKeys(AuthoredDepth& authored,
                                                    const std::vector<KeyRef>& keys) {
    const bool all_hold = std::ranges::all_of(keys, [&authored](const KeyRef& key) {
        const Track* track = TrackNamed(authored, key.property);
        if (track == nullptr) return false;
        const auto found = std::ranges::find(track->keys, key.frame, &Keyframe::frame);
        return found != track->keys.end() && found->ease == Ease::Hold;
    });
    return SetKeysEase(authored, keys, all_hold ? Ease::Linear : Ease::Hold, {});
}

}
