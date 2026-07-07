#include <catch2/catch_test_macros.hpp>

#include "loop/modern_loop.h"

#include <cstdint>

namespace {

struct State {
    uint32_t mc_prev_cur = Loop::kNoPrevCur;
    int loops_done = 0;
    int label_seen = 0;
};

Loop::ModernDecision Step(State& s, const Loop::ModernTick& t) {
    return Loop::StepModernLoop(t, s.mc_prev_cur, s.loops_done, s.label_seen);
}

}

TEST_CASE("modern master cycle finishes at loop_count timelines") {
    State s;
    Loop::ModernTick t;
    t.have_pos = true;
    t.total_len = 1200;
    t.loop_count = 1;

    t.cur_pos = 600;
    CHECK_FALSE(Step(s, t).naturally_done);
    t.cur_pos = 1200;
    CHECK(Step(s, t).naturally_done);
}

TEST_CASE("modern master cycle counts N timelines for loop_count > 1") {
    State s;
    Loop::ModernTick t;
    t.have_pos = true;
    t.total_len = 1000;
    t.loop_count = 2;

    t.cur_pos = 1000;
    CHECK_FALSE(Step(s, t).naturally_done);
    t.cur_pos = 2000;
    CHECK(Step(s, t).naturally_done);
}

TEST_CASE("modern hold mode ignores the free-running cur and uses wrap count") {
    State s;
    Loop::ModernTick t;
    t.have_pos = true;
    t.total_len = 1000;
    t.loop_count = 1;
    t.hold_mode = true;

    t.cur_pos = 5000;
    CHECK_FALSE(Step(s, t).naturally_done);

    t.mc_valid = true;
    t.mc_cur = 500;
    CHECK_FALSE(Step(s, t).naturally_done);
    t.mc_cur = 100;
    const Loop::ModernDecision wrap = Step(s, t);
    CHECK(wrap.wrapped);
    CHECK(wrap.naturally_done);
    CHECK(s.loops_done == 1);
}

TEST_CASE("modern label export stops after loop_count backward wraps") {
    State s;
    Loop::ModernTick t;
    t.label_active = true;
    t.mc_valid = true;
    t.loop_count = 1;

    t.mc_cur = 0;
    CHECK_FALSE(Step(s, t).naturally_done);
    t.mc_cur = 200;
    CHECK_FALSE(Step(s, t).naturally_done);
    t.mc_cur = 120;
    const Loop::ModernDecision d = Step(s, t);
    CHECK(d.wrapped);
    CHECK(d.naturally_done);
    CHECK(s.loops_done == 1);
    CHECK(s.label_seen == 3);
}

TEST_CASE("modern first mc read seeds the baseline without counting") {
    State s;
    Loop::ModernTick t;
    t.mc_valid = true;
    t.mc_cur = 500;
    const Loop::ModernDecision d = Step(s, t);
    CHECK_FALSE(d.wrapped);
    CHECK(s.loops_done == 0);
    CHECK(s.mc_prev_cur == 500);
}

TEST_CASE("modern one-shot finishes when output freezes at total") {
    State s;
    Loop::ModernTick t;
    t.have_pos = true;
    t.total_len = 1000;
    t.loop_count = 3;
    t.cur_pos = 1000;

    t.idle_frames = 7;
    const Loop::ModernDecision below = Step(s, t);
    CHECK_FALSE(below.master_oneshot);
    CHECK_FALSE(below.naturally_done);

    t.idle_frames = 8;
    const Loop::ModernDecision froze = Step(s, t);
    CHECK(froze.master_oneshot);
    CHECK(froze.naturally_done);
}

TEST_CASE("modern fallback uses master-complete when position is unavailable") {
    State s;
    Loop::ModernTick t;
    t.have_pos = false;

    t.is_master_complete = false;
    CHECK_FALSE(Step(s, t).naturally_done);
    t.is_master_complete = true;
    CHECK(Step(s, t).naturally_done);
}
