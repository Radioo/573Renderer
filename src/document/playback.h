#pragma once

#include "formats/afp_animation.h"

#include <cstdint>

namespace Document {

inline constexpr double kSlowestFrameRate = 1.0;
inline constexpr double kFastestFrameRate = 240.0;
inline constexpr double kFallbackFrameRate = 60.0;

struct Playback {
    uint32_t frame_count = 0;
    bool looping = true;
};

struct Step {
    uint32_t frame = 0;
    bool playing = false;
};

[[nodiscard]] double FrameRate(const AfpAnimation::Animation& animation);

[[nodiscard]] int FrameIntervalMs(double rate);

[[nodiscard]] Step Advance(const Playback& playback, uint32_t frame);

}
