#include "document/playback.h"

#include "formats/afp_animation.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>

namespace Document {

namespace {

constexpr uint32_t kFixedPointRate = 0x2;
constexpr double kFixedPointScale = 1024.0;
constexpr double kMillisecondsPerSecond = 1000.0;

}

double FrameRate(const AfpAnimation::Animation& animation) {
    const double stored =
        (animation.flags & kFixedPointRate) != 0
            ? static_cast<double>(std::bit_cast<int32_t>(animation.fps)) / kFixedPointScale
            : static_cast<double>(std::bit_cast<float>(animation.fps));
    if (!std::isfinite(stored) || stored < kSlowestFrameRate || stored > kFastestFrameRate)
        return kFallbackFrameRate;
    return stored;
}

int FrameIntervalMs(double rate) {
    const double clamped = std::clamp(rate, kSlowestFrameRate, kFastestFrameRate);
    return std::max(1, static_cast<int>(std::lround(kMillisecondsPerSecond / clamped)));
}

Step Advance(const Playback& playback, uint32_t frame) {
    if (playback.frame_count == 0) return Step{.frame = 0, .playing = false};
    uint32_t first = 0;
    uint32_t last = playback.frame_count - 1;
    if (playback.work_area) {
        first = std::min(playback.work_area->first_frame, last);
        last = std::clamp(playback.work_area->last_frame, first, last);
        if (frame < first || frame > last) return Step{.frame = first, .playing = true};
    }
    if (frame >= last) {
        if (!playback.looping) return Step{.frame = last, .playing = false};
        return Step{.frame = first, .playing = true};
    }
    return Step{.frame = frame + 1, .playing = true};
}

WorkArea WithWorkAreaStart(const std::optional<WorkArea>& area, uint32_t frame,
                           uint32_t frame_count) {
    const uint32_t last = frame_count == 0 ? 0 : frame_count - 1;
    const uint32_t start = std::min(frame, last);
    const uint32_t end = area ? std::min(area->last_frame, last) : last;
    return WorkArea{.first_frame = start, .last_frame = std::max(start, end)};
}

WorkArea WithWorkAreaEnd(const std::optional<WorkArea>& area, uint32_t frame,
                         uint32_t frame_count) {
    const uint32_t last = frame_count == 0 ? 0 : frame_count - 1;
    const uint32_t end = std::min(frame, last);
    const uint32_t start = area ? area->first_frame : 0;
    return WorkArea{.first_frame = std::min(start, end), .last_frame = end};
}

}
