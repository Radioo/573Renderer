#include "judge/judge_clock.h"

#include <algorithm>
#include <cmath>

namespace Judge {

namespace {

constexpr int kMostStepsPerTick = 4;
constexpr float kWholeStepShare = 0.999F;

}

int Clock::Tick(float real_seconds) {
    if (paused_ || !(step_ > 0.0F) || !(real_seconds > 0.0F)) return 0;
    since_advance_ += real_seconds * Scale();
    const auto due = static_cast<int>(std::floor(since_advance_ / step_));
    since_advance_ -= static_cast<float>(due) * step_;
    since_advance_ = std::clamp(since_advance_, 0.0F, step_);
    if (due > kMostStepsPerTick) {
        since_advance_ = 0.0F;
        return kMostStepsPerTick;
    }
    return due;
}

int Clock::StepRender() {
    if (!(step_ > 0.0F)) return 0;
    since_advance_ += step_ / static_cast<float>(kRendersPerLogicStep);
    if (since_advance_ < step_ * kWholeStepShare) return 0;
    since_advance_ = std::max(0.0F, since_advance_ - step_);
    return 1;
}

void Clock::Restart() {
    since_advance_ = 0.0F;
}

void Clock::SetScaleIndex(std::size_t index) {
    scale_index_ = std::min(index, kTimeScales.size() - 1);
}

}
