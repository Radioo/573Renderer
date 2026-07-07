#pragma once

#include <cstdint>

namespace Loop {

constexpr uint32_t kNoPrevCur = 0xFFFFFFFF;

struct ModernTick {
    bool have_pos = false;
    uint32_t cur_pos = 0;
    uint32_t total_len = 0;
    bool mc_valid = false;
    uint32_t mc_cur = 0;
    bool is_master_complete = false;
    int idle_frames = 0;
    bool label_active = false;
    int loop_count = 1;
    bool hold_mode = false;
};

struct ModernDecision {
    bool wrapped = false;
    bool naturally_done = false;
    bool master_oneshot = false;
};

ModernDecision StepModernLoop(const ModernTick& in, uint32_t& mc_prev_cur, int& loops_done,
                              int& label_seen);

}
