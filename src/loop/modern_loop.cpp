#include "loop/modern_loop.h"

#include <cstdint>

namespace Loop {

namespace {
constexpr int kOneshotIdleFrames = 8;
}

ModernDecision StepModernLoop(const ModernTick& in, uint32_t& mc_prev_cur, int& loops_done,
                              int& label_seen) {
    ModernDecision out;

    if (in.mc_valid) {
        if (mc_prev_cur != kNoPrevCur && in.mc_cur < mc_prev_cur) {
            loops_done++;
            out.wrapped = true;
        }
        mc_prev_cur = in.mc_cur;
    }
    if (in.label_active) label_seen++;

    const bool label_done = in.label_active && loops_done >= in.loop_count;
    out.master_oneshot = in.have_pos && in.total_len > 0 && in.cur_pos >= in.total_len &&
                         in.idle_frames >= kOneshotIdleFrames;

    if (in.label_active) {
        out.naturally_done = label_done;
    } else if (in.have_pos && in.total_len > 0) {
        const uint32_t target = static_cast<uint32_t>(in.loop_count) * in.total_len;
        out.naturally_done = loops_done >= in.loop_count ||
                             (!in.hold_mode && in.cur_pos >= target) || out.master_oneshot;
    } else {
        out.naturally_done = in.is_master_complete;
    }
    return out;
}

}
