#include "document/key_simplify.h"

#include "document/authored.h"
#include "document/key_selection.h"
#include "document/keyframes.h"
#include "document/placement_values.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Document {

namespace {

bool Within(const std::vector<int64_t>& a, const std::vector<int64_t>& b, int64_t tolerance) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); i++) {
        if (std::abs(a[i] - b[i]) > tolerance) return false;
    }
    return true;
}

bool LineReaches(const Track& original, std::size_t from, std::size_t to, int64_t tolerance) {
    Track line{.property = original.property, .keys = {original.keys[from], original.keys[to]}};
    line.keys.front().ease = Ease::Linear;
    for (uint32_t frame = line.keys.front().frame + 1; frame < line.keys.back().frame; frame++) {
        if (!Within(SampleTrack(line, frame), SampleTrack(original, frame), tolerance))
            return false;
    }
    return true;
}

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

std::vector<Keyframe> Thinned(const Track& original, std::size_t first, std::size_t last,
                              int64_t tolerance) {
    std::vector<Keyframe> kept{original.keys[first]};
    std::size_t at = first;
    while (at < last) {
        std::size_t reach = at + 1;
        while (reach < last && LineReaches(original, at, reach + 1, tolerance))
            reach++;
        if (reach > at + 1) kept.back().ease = Ease::Linear;
        kept.push_back(original.keys[reach]);
        at = reach;
    }
    return kept;
}

}

Support::Expected<std::vector<KeyRef>, std::string>
SimplifyKeys(AuthoredDepth& authored, const std::vector<KeyRef>& keys, int64_t tolerance) {
    if (keys.empty()) return Support::Unexpected(std::string("no keyframes are selected"));
    if (tolerance < 0) return Support::Unexpected(std::string("a tolerance cannot be below 0"));
    AuthoredDepth edited = authored;
    std::vector<KeyRef> remaining;
    std::size_t removed = 0;
    std::size_t matched = 0;
    for (Track& track : edited.tracks) {
        const std::set<uint32_t> selected = SelectedFrames(track, keys);
        if (selected.empty()) continue;
        matched += selected.size();
        auto run = CheckRun(track, selected);
        if (!run) return Support::Unexpected(run.error());
        if (PropertyIsStepped(track.property)) {
            for (const uint32_t frame : selected)
                remaining.push_back(KeyRef{.property = track.property, .frame = frame});
            continue;
        }
        const Track original = track;
        const auto first = static_cast<std::size_t>(
            std::ranges::find(original.keys, *selected.begin(), &Keyframe::frame) -
            original.keys.begin());
        const auto last = static_cast<std::size_t>(
            std::ranges::find(original.keys, *selected.rbegin(), &Keyframe::frame) -
            original.keys.begin());
        std::vector<Keyframe> kept = Thinned(original, first, last, tolerance);
        removed += (last - first + 1) - kept.size();
        for (const Keyframe& key : kept)
            remaining.push_back(KeyRef{.property = track.property, .frame = key.frame});
        track.keys.erase(track.keys.begin() + static_cast<std::ptrdiff_t>(first),
                         track.keys.begin() + static_cast<std::ptrdiff_t>(last + 1));
        track.keys.insert(track.keys.begin() + static_cast<std::ptrdiff_t>(first), kept.begin(),
                          kept.end());
    }
    if (matched != std::set<KeyRef>(keys.begin(), keys.end()).size()) {
        return Support::Unexpected(std::string("a selected keyframe belongs to no property"));
    }
    if (removed == 0) {
        return Support::Unexpected(
            std::string("nothing can be simplified: every selected keyframe is needed"));
    }
    authored = std::move(edited);
    return remaining;
}

}
