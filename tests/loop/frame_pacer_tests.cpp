#include <catch2/catch_test_macros.hpp>

#include "loop/frame_pacer.h"

#include <cstdint>

namespace {
constexpr int64_t kFreq = 10'000'000;
}

TEST_CASE("frame ticks derive from fps and clamp out-of-range rates") {
    CHECK(Loop::FramePacer(100, kFreq).FrameTicks() == 100'000);
    CHECK(Loop::FramePacer(0, kFreq).FrameTicks() == kFreq);
    CHECK(Loop::FramePacer(-5, kFreq).FrameTicks() == kFreq);
    CHECK(Loop::FramePacer(5000, kFreq).FrameTicks() == kFreq / 1000);
}

TEST_CASE("PlanWait proceeds once the target tick is reached or passed") {
    Loop::FramePacer p(100, kFreq);
    p.AnchorTo(1000);
    CHECK(p.PlanWait(1000).action == Loop::FramePacer::Wait::Proceed);
    CHECK(p.PlanWait(2000).action == Loop::FramePacer::Wait::Proceed);
}

TEST_CASE("PlanWait sleeps when far from the target and spins when close") {
    Loop::FramePacer p(100, kFreq);
    p.AnchorTo(0);
    p.ScheduleNext();
    const Loop::FramePacer::Plan far = p.PlanWait(0);
    CHECK(far.action == Loop::FramePacer::Wait::Sleep);
    CHECK(far.sleep_ms == 9);
    CHECK(p.PlanWait(96'000).action == Loop::FramePacer::Wait::Spin);
}

TEST_CASE("PlanWait sleep floors to zero milliseconds just past the spin threshold") {
    Loop::FramePacer p(100, kFreq);
    p.AnchorTo(0);
    p.ScheduleNext();
    const int64_t busy = kFreq / 2000;
    const Loop::FramePacer::Plan tiny = p.PlanWait(100'000 - busy - 1);
    CHECK(tiny.action == Loop::FramePacer::Wait::Sleep);
    CHECK(tiny.sleep_ms == 0);
}

TEST_CASE("ScheduleNext accumulates whole frames and AnchorTo resets") {
    Loop::FramePacer p(100, kFreq);
    p.AnchorTo(0);
    p.ScheduleNext();
    p.ScheduleNext();
    p.ScheduleNext();
    CHECK(p.TargetTick() == 300'000);
    p.AnchorTo(5000);
    CHECK(p.TargetTick() == 5000);
}

TEST_CASE("ResyncIfBehind drops a pacing debt beyond four frames") {
    Loop::FramePacer p(100, kFreq);
    p.AnchorTo(0);
    p.ResyncIfBehind(300'000);
    CHECK(p.TargetTick() == 0);
    p.ResyncIfBehind(400'001);
    CHECK(p.TargetTick() == 400'001);
}
