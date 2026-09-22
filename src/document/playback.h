#pragma once

#include "formats/afp_animation.h"

#include <cstdint>
#include <optional>

namespace Document {

inline constexpr double kSlowestFrameRate = 1.0;
inline constexpr double kFastestFrameRate = 240.0;
inline constexpr double kFallbackFrameRate = 60.0;

struct WorkArea {
    uint32_t first_frame = 0;
    uint32_t last_frame = 0;

    friend bool operator==(const WorkArea&, const WorkArea&) = default;
};

struct Playback {
    uint32_t frame_count = 0;
    bool looping = true;
    std::optional<WorkArea> work_area;
};

struct Step {
    uint32_t frame = 0;
    bool playing = false;
};

[[nodiscard]] double FrameRate(const AfpAnimation::Animation& animation);

[[nodiscard]] int FrameIntervalMs(double rate);

[[nodiscard]] Step Advance(const Playback& playback, uint32_t frame);

[[nodiscard]] WorkArea WithWorkAreaStart(const std::optional<WorkArea>& area, uint32_t frame,
                                         uint32_t frame_count);

[[nodiscard]] WorkArea WithWorkAreaEnd(const std::optional<WorkArea>& area, uint32_t frame,
                                       uint32_t frame_count);

}
