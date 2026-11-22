#pragma once

#include "judge/afp_pose.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>

namespace AfpInterp {

struct LayerTiming {
    float fps = 0.0F;
    float speed = 1.0F;
    bool frozen = false;
    float accumulator = 0.0F;
};

[[nodiscard]] int FrameSteps(const LayerTiming& after, float dt, float accumulator_before);

[[nodiscard]] float Alpha(const LayerTiming& now, float seconds_since_advance);

struct Sample {
    std::uintptr_t object = 0;
    std::uintptr_t parent = 0;
    bool script_controlled = false;
    std::uint16_t created = 0;
    std::uint16_t lifetime_end = 0;
    AfpPose::Pose pose;
};

class LayerTracks {
public:
    void BeginAdvance(std::span<const Sample> before);
    void MarkJumped(std::uintptr_t clip);
    void MarkGoto(std::uintptr_t clip, std::uint16_t from_frame, std::uint16_t to_frame);
    void Forget(std::uintptr_t object);
    void EndAdvance(int steps, std::span<const Sample> after);

    [[nodiscard]] std::optional<AfpPose::Pose> DrawPose(std::uintptr_t object,
                                                        const AfpPose::Pose& live, float alpha,
                                                        AfpPose::MatrixLerp mode) const;
    [[nodiscard]] std::size_t MovingCount() const;

private:
    struct Track {
        AfpPose::Pose from;
        AfpPose::Pose to;
    };

    std::unordered_map<std::uintptr_t, Track> tracks_;
    std::unordered_map<std::uintptr_t, AfpPose::Pose> before_;
    std::unordered_set<std::uintptr_t> jumped_;
    bool in_advance_ = false;
};

}
