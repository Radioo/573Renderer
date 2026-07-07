#pragma once

#include <cstdint>

namespace Loop {

class FramePacer {
public:
    enum class Wait { Proceed, Sleep, Spin };

    struct Plan {
        Wait action = Wait::Proceed;
        uint32_t sleep_ms = 0;
    };

    FramePacer(int fps, int64_t qpc_freq);

    void AnchorTo(int64_t now);
    void ScheduleNext();
    [[nodiscard]] Plan PlanWait(int64_t now) const;
    void ResyncIfBehind(int64_t now);

    [[nodiscard]] int64_t FrameTicks() const { return frame_ticks_; }
    [[nodiscard]] int64_t TargetTick() const { return target_tick_; }

private:
    int64_t qpc_freq_;
    int64_t frame_ticks_;
    int64_t busy_wait_ticks_;
    int64_t target_tick_ = 0;
};

}
