#include "document/key_wiggle.h"

#include "document/authored.h"
#include "document/key_selection.h"
#include "document/keyframes.h"
#include "document/placement_values.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Document {

namespace {

std::set<uint32_t> SelectedFrames(const Track& track, const std::vector<KeyRef>& keys) {
    std::set<uint32_t> frames;
    for (const KeyRef& key : keys) {
        if (key.property == track.property) frames.insert(key.frame);
    }
    return frames;
}

Support::Expected<void, std::string> CheckRun(const Track& track,
                                              const std::set<uint32_t>& selected) {
    for (const uint32_t frame : selected) {
        if (std::ranges::find(track.keys, frame, &Keyframe::frame) == track.keys.end()) {
            return Support::Unexpected("there is no " + track.property + " keyframe at frame " +
                                       std::to_string(frame));
        }
    }
    for (const Keyframe& key : track.keys) {
        const bool inside = key.frame > *selected.begin() && key.frame < *selected.rbegin();
        if (inside && !selected.contains(key.frame)) {
            return Support::Unexpected("the " + track.property + " keyframe at frame " +
                                       std::to_string(key.frame) +
                                       " lies between the selected ones and is not selected");
        }
    }
    return {};
}

int64_t Offset(std::mt19937& engine, int64_t magnitude) {
    const auto choices = static_cast<uint64_t>((2 * magnitude) + 1);
    return static_cast<int64_t>(static_cast<uint64_t>(engine()) % choices) - magnitude;
}

Support::Expected<void, std::string> WiggleTrack(Track& track, const std::set<uint32_t>& selected,
                                                 const Wiggle& wiggle, std::mt19937& engine) {
    const uint32_t first = *selected.begin();
    const uint32_t last = *selected.rbegin();
    if (first + wiggle.every >= last) {
        return Support::Unexpected("there is no room for a wiggle every " +
                                   std::to_string(wiggle.every) + " frames between frames " +
                                   std::to_string(first) + " and " + std::to_string(last));
    }
    const Track original = track;
    std::erase_if(track.keys, [first, last](const Keyframe& key) {
        return key.frame > first && key.frame < last;
    });
    for (uint32_t frame = first + wiggle.every; frame < last; frame += wiggle.every) {
        std::vector<int64_t> value = SampleTrack(original, frame);
        for (int64_t& part : value)
            part += Offset(engine, wiggle.magnitude);
        auto added = AddKeyframe(
            track, Keyframe{.frame = frame, .value = value, .ease = Ease::Linear, .bezier = {}});
        if (!added) return Support::Unexpected(added.error());
    }
    return {};
}

}

Support::Expected<std::vector<KeyRef>, std::string>
WiggleKeys(AuthoredDepth& authored, const std::vector<KeyRef>& keys, const Wiggle& wiggle) {
    if (keys.empty()) return Support::Unexpected(std::string("no keyframes are selected"));
    if (wiggle.every == 0)
        return Support::Unexpected(std::string("a wiggle needs a keyframe every 1 frame or more"));
    if (wiggle.magnitude <= 0)
        return Support::Unexpected(std::string("a wiggle needs a magnitude above 0"));
    std::mt19937 engine(wiggle.seed);
    AuthoredDepth edited = authored;
    std::vector<KeyRef> wiggled;
    std::size_t matched = 0;
    for (Track& track : edited.tracks) {
        const std::set<uint32_t> selected = SelectedFrames(track, keys);
        if (selected.empty()) continue;
        matched += selected.size();
        auto run = CheckRun(track, selected);
        if (!run) return Support::Unexpected(run.error());
        if (selected.size() < 2 || PropertyIsStepped(track.property)) continue;
        auto shaken = WiggleTrack(track, selected, wiggle, engine);
        if (!shaken) return Support::Unexpected(shaken.error());
        for (const Keyframe& key : track.keys) {
            if (key.frame >= *selected.begin() && key.frame <= *selected.rbegin())
                wiggled.push_back(KeyRef{.property = track.property, .frame = key.frame});
        }
    }
    if (matched != std::set<KeyRef>(keys.begin(), keys.end()).size()) {
        return Support::Unexpected(std::string("a selected keyframe belongs to no property"));
    }
    if (wiggled.empty()) {
        return Support::Unexpected(
            std::string("wiggling needs two selected keyframes of one property that moves"));
    }
    authored = std::move(edited);
    return wiggled;
}

}
