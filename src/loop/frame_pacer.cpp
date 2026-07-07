#include "loop/frame_pacer.h"

#include <algorithm>
#include <cstdint>

namespace Loop {

namespace {
constexpr int kMinFps = 1;
constexpr int kMaxFps = 1000;
constexpr int64_t kBusyWaitHz = 2000;
constexpr int64_t kResyncFrames = 4;
}

FramePacer::FramePacer(int fps, int64_t qpc_freq)
    : qpc_freq_(std::max<int64_t>(1, qpc_freq)),
      frame_ticks_(std::max<int64_t>(1, qpc_freq_ / std::clamp(fps, kMinFps, kMaxFps))),
      busy_wait_ticks_(std::max<int64_t>(1, qpc_freq_ / kBusyWaitHz)) {}

void FramePacer::AnchorTo(int64_t now) {
    target_tick_ = now;
}

void FramePacer::ScheduleNext() {
    target_tick_ += frame_ticks_;
}

FramePacer::Plan FramePacer::PlanWait(int64_t now) const {
    const int64_t remaining = target_tick_ - now;
    if (remaining <= 0) return {.action = Wait::Proceed, .sleep_ms = 0};
    if (remaining > busy_wait_ticks_) {
        const int64_t ms = (remaining - busy_wait_ticks_) * 1000 / qpc_freq_;
        return {.action = Wait::Sleep, .sleep_ms = static_cast<uint32_t>(std::max<int64_t>(0, ms))};
    }
    return {.action = Wait::Spin, .sleep_ms = 0};
}

void FramePacer::ResyncIfBehind(int64_t now) {
    if (now - target_tick_ > frame_ticks_ * kResyncFrames) target_tick_ = now;
}

}
