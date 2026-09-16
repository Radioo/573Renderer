#include "document/playback.h"

#include "formats/afp_animation.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

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
    const uint32_t last = playback.frame_count - 1;
    if (frame >= last) {
        if (!playback.looping) return Step{.frame = last, .playing = false};
        return Step{.frame = 0, .playing = true};
    }
    return Step{.frame = frame + 1, .playing = true};
}

}
