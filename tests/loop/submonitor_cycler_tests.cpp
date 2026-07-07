#include <catch2/catch_test_macros.hpp>

#include "loop/submonitor_cycler.h"

TEST_CASE("DissolveCycler seeds on the first tick without advancing") {
    Loop::DissolveCycler c(100);
    const Loop::DissolveCycler::Tick t = c.Advance(40, 200);
    CHECK_FALSE(t.cycle_changed);
    CHECK(t.cycle == 0);
}

TEST_CASE("DissolveCycler accumulates forward playhead deltas into cycles") {
    Loop::DissolveCycler c(100);
    c.Advance(0, 200);
    CHECK_FALSE(c.Advance(50, 200).cycle_changed);
    const Loop::DissolveCycler::Tick t = c.Advance(120, 200);
    CHECK(t.cycle_changed);
    CHECK(t.cycle == 1);
    CHECK_FALSE(c.Advance(150, 200).cycle_changed);
    const Loop::DissolveCycler::Tick t2 = c.Advance(210, 200);
    CHECK(t2.cycle_changed);
    CHECK(t2.cycle == 2);
}

TEST_CASE("DissolveCycler counts the wrapped span when the playhead loops") {
    Loop::DissolveCycler c(40);
    c.Advance(180, 200);
    const Loop::DissolveCycler::Tick t = c.Advance(30, 200);
    CHECK(t.cycle_changed);
    CHECK(t.cycle == 1);
}

TEST_CASE("DissolveCycler clamps a zero loop length to avoid divide by zero") {
    Loop::DissolveCycler c(0);
    c.Advance(0, 200);
    const Loop::DissolveCycler::Tick t = c.Advance(3, 200);
    CHECK(t.cycle_changed);
    CHECK(t.cycle == 3);
}

TEST_CASE("FadeCycler holds cycle zero and fades it out before the period ends") {
    Loop::FadeCycler c(3, 4);
    for (int i = 0; i < 7; ++i) {
        const Loop::FadeCycler::Tick t = c.Advance();
        CHECK_FALSE(t.fade_in);
        CHECK_FALSE(t.fade_out);
    }
    const Loop::FadeCycler::Tick out = c.Advance();
    CHECK(out.fade_out);
    CHECK(out.cycle == 0);
    for (int i = 0; i < 2; ++i) {
        const Loop::FadeCycler::Tick t = c.Advance();
        CHECK_FALSE(t.fade_out);
    }
}

TEST_CASE("FadeCycler fades in each new cycle exactly once") {
    Loop::FadeCycler c(3, 4);
    for (int i = 0; i < 10; ++i)
        c.Advance();
    const Loop::FadeCycler::Tick in = c.Advance();
    CHECK(in.fade_in);
    CHECK(in.cycle == 1);
    for (int i = 0; i < 6; ++i)
        CHECK_FALSE(c.Advance().fade_in);
    const Loop::FadeCycler::Tick out = c.Advance();
    CHECK(out.fade_out);
    CHECK(out.cycle == 1);
}
