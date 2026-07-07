#include "loop/submonitor_cycler.h"

#include <algorithm>
#include <cstdint>

namespace Loop {

DissolveCycler::DissolveCycler(int loop_frames) : loop_frames_(std::max(1, loop_frames)) {}

DissolveCycler::Tick DissolveCycler::Advance(uint32_t cur, uint32_t total) {
    if (prev_cur_ == kUnseeded) {
        prev_cur_ = cur;
        return {.cycle_changed = false, .cycle = cycle_};
    }
    if (cur >= prev_cur_) {
        frames_ += static_cast<int64_t>(cur - prev_cur_);
    } else {
        if (total > 0) frames_ += static_cast<int64_t>(total - prev_cur_);
        frames_ += static_cast<int64_t>(cur);
    }
    prev_cur_ = cur;
    const int cyc = static_cast<int>(frames_ / loop_frames_);
    if (cyc != cycle_) {
        cycle_ = cyc;
        return {.cycle_changed = true, .cycle = cyc};
    }
    return {.cycle_changed = false, .cycle = cyc};
}

FadeCycler::FadeCycler(int fade_frames, int dwell_frames)
    : period_(std::max(1, fade_frames + dwell_frames + fade_frames)), fade_(fade_frames) {}

FadeCycler::Tick FadeCycler::Advance() {
    const int64_t t = count_ % period_;
    const int cyc = static_cast<int>(count_ / period_);
    Tick out;
    out.cycle = cyc;
    if (cyc != cycle_) {
        cycle_ = cyc;
        fade_out_fired_ = false;
        out.fade_in = true;
    } else if (!fade_out_fired_ && t >= static_cast<int64_t>(period_ - fade_)) {
        fade_out_fired_ = true;
        out.fade_out = true;
    }
    count_++;
    return out;
}

}
