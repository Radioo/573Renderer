#include <catch2/catch_test_macros.hpp>

#include "loop/blend_loop.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

std::vector<uint8_t> SolidRgba(int px, uint8_t v) {
    return std::vector<uint8_t>(static_cast<std::size_t>(px) * 4, v);
}

}

TEST_CASE("PlanBlendLoop trims a trailing frozen run") {
    Loop::FrameBuffer buf;
    for (int i = 0; i < 40; i++)
        buf.push_back(SolidRgba(4, static_cast<uint8_t>(i)));
    for (int i = 0; i < 5; i++)
        buf.push_back(SolidRgba(4, 200));
    const std::size_t before = buf.size();
    const Loop::BlendPlan plan = Loop::PlanBlendLoop(buf, 0);
    CHECK(buf.size() == before - 4);
    CHECK(plan.loop_length == 30);
}

TEST_CASE("PlanBlendLoop picks the length whose frame best matches frame 0") {
    Loop::FrameBuffer buf;
    for (int i = 0; i < 50; i++)
        buf.push_back(SolidRgba(4, static_cast<uint8_t>(100 + i)));
    buf.push_back(SolidRgba(4, 100));
    buf.push_back(SolidRgba(4, 210));
    const Loop::BlendPlan plan = Loop::PlanBlendLoop(buf, 0);
    CHECK(plan.loop_length == 50);
    CHECK(plan.best_mad == 0.0);
}

TEST_CASE("PlanBlendLoop returns an empty plan for too-short input") {
    Loop::FrameBuffer buf;
    buf.push_back(SolidRgba(4, 0));
    const Loop::BlendPlan plan = Loop::PlanBlendLoop(buf, 15);
    CHECK(plan.loop_length == 0);
}

TEST_CASE("PlanBlendLoop clamps the crossfade to fit the buffer") {
    Loop::FrameBuffer buf;
    for (int i = 0; i < 35; i++)
        buf.push_back(SolidRgba(4, static_cast<uint8_t>(i)));
    const Loop::BlendPlan plan = Loop::PlanBlendLoop(buf, 15);
    CHECK(plan.loop_length + plan.crossfade <= static_cast<int>(buf.size()));
}

TEST_CASE("ComposeBlendFrame passes body frames through unchanged") {
    Loop::FrameBuffer buf;
    for (int i = 0; i < 40; i++)
        buf.push_back(SolidRgba(2, static_cast<uint8_t>(i)));
    Loop::BlendPlan plan;
    plan.loop_length = 40;
    plan.crossfade = 4;
    std::vector<uint8_t> out(8, 0);
    Loop::ComposeBlendFrame(buf, plan, 10, out);
    CHECK(out[0] == 10);
    CHECK(out[7] == 10);
}

TEST_CASE("ComposeBlendFrame crossfades from the tail toward the start") {
    Loop::FrameBuffer buf;
    for (int i = 0; i < 40; i++)
        buf.push_back(SolidRgba(1, 0));
    for (int i = 20; i < 24; i++)
        buf[static_cast<std::size_t>(i)] = SolidRgba(1, 200);
    Loop::BlendPlan plan;
    plan.loop_length = 20;
    plan.crossfade = 4;

    std::vector<uint8_t> out(4, 0);
    Loop::ComposeBlendFrame(buf, plan, 0, out);
    CHECK(out[0] > 100);

    Loop::ComposeBlendFrame(buf, plan, 3, out);
    CHECK(out[0] < 100);
}
