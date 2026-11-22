#include "judge/interp_tracks.h"
#include "judge/interp_layout.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace AfpInterp {

namespace {

constexpr float kWholeAlpha = 1.0F;

}

int FrameSteps(const LayerTiming& after, float dt, float accumulator_before) {
    if (!(after.fps > 0.0F)) return -1;
    const float advanced = after.frozen ? 0.0F : std::fabs(after.speed * dt);
    return static_cast<int>(
        std::lround((accumulator_before + advanced - after.accumulator) * after.fps));
}

float Alpha(const LayerTiming& now, float seconds_since_advance) {
    if (!(now.fps > 0.0F)) return kWholeAlpha;
    const float ahead = now.frozen ? 0.0F : seconds_since_advance * std::fabs(now.speed);
    return std::clamp((now.accumulator + ahead) * now.fps, 0.0F, kWholeAlpha);
}

void LayerTracks::BeginAdvance(std::span<const Sample> before) {
    before_.clear();
    jumped_.clear();
    for (const Sample& sample : before)
        before_.emplace(sample.object, sample.pose);
    in_advance_ = true;
}

void LayerTracks::MarkJumped(std::uintptr_t clip) {
    if (in_advance_) jumped_.insert(clip);
}

void LayerTracks::MarkGoto(std::uintptr_t clip, std::uint16_t from_frame, std::uint16_t to_frame) {
    if (static_cast<int>(to_frame) != static_cast<int>(from_frame) + 1) MarkJumped(clip);
}

void LayerTracks::Forget(std::uintptr_t object) {
    tracks_.erase(object);
    before_.erase(object);
}

void LayerTracks::EndAdvance(int steps, std::span<const Sample> after) {
    std::unordered_map<std::uintptr_t, Track> next;
    next.reserve(after.size());
    for (const Sample& sample : after) {
        const bool parent_jumped = jumped_.contains(sample.parent);
        const auto before = before_.find(sample.object);
        const auto kept = tracks_.find(sample.object);
        const bool stable = !sample.script_controlled && !parent_jumped;
        if (steps == 1 && stable && before != before_.end() &&
            AfpPose::Interpolable(before->second, sample.pose)) {
            next.emplace(sample.object, Track{.from = before->second, .to = sample.pose});
        } else if (steps == 0 && stable && kept != tracks_.end() &&
                   kept->second.to == sample.pose) {
            next.emplace(sample.object, kept->second);
        } else {
            next.emplace(sample.object, Track{.from = sample.pose, .to = sample.pose});
        }
    }
    tracks_ = std::move(next);
    before_.clear();
    jumped_.clear();
    in_advance_ = false;
}

std::optional<AfpPose::Pose> LayerTracks::DrawPose(std::uintptr_t object, const AfpPose::Pose& live,
                                                   float alpha, AfpPose::MatrixLerp mode) const {
    const auto found = tracks_.find(object);
    if (found == tracks_.end()) return std::nullopt;
    const Track& track = found->second;
    if (track.from == track.to || !(live == track.to)) return std::nullopt;
    return AfpPose::Lerp(track.from, track.to, alpha, mode);
}

std::size_t LayerTracks::MovingCount() const {
    return static_cast<std::size_t>(std::ranges::count_if(
        tracks_, [](const auto& entry) { return !(entry.second.from == entry.second.to); }));
}

std::string PrologueMismatch(const Layout& layout, const ReadImage& read) {
    for (const HookSite* site : {&layout.layer_advance, &layout.layer_draw, &layout.object_init,
                                 &layout.clip_goto, &layout.clip_set_frame}) {
        std::vector<std::uint8_t> live(site->prologue_size);
        if (!read(site->rva, live.size(), live.data())) {
            return std::string(site->name) + " lies outside the loaded libafp image";
        }
        if (std::memcmp(live.data(), site->prologue.data(), live.size()) != 0) {
            return std::string("the prologue of ") + site->name +
                   " does not match this profile's libafp build";
        }
    }
    return {};
}

}
