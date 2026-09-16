#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/playback.h"
#include "formats/afp_animation.h"

#include <bit>
#include <cstdint>

namespace {

AfpAnimation::Animation FixedPoint(int32_t stored) {
    AfpAnimation::Animation animation;
    animation.flags = 0x2;
    animation.fps = std::bit_cast<uint32_t>(stored);
    return animation;
}

AfpAnimation::Animation Floating(float stored) {
    AfpAnimation::Animation animation;
    animation.flags = 0;
    animation.fps = std::bit_cast<uint32_t>(stored);
    return animation;
}

}

TEST_CASE("A fixed point frame rate is the stored number over 1024") {
    CHECK(Document::FrameRate(FixedPoint(60 * 1024)) == Catch::Approx(60.0));
    CHECK(Document::FrameRate(FixedPoint(30 * 1024)) == Catch::Approx(30.0));
    CHECK(Document::FrameRate(FixedPoint((24 * 1024) + 512)) == Catch::Approx(24.5));
}

TEST_CASE("A frame rate with no fixed point flag is the stored float") {
    CHECK(Document::FrameRate(Floating(60.0F)) == Catch::Approx(60.0));
    CHECK(Document::FrameRate(Floating(29.97F)) == Catch::Approx(29.97).margin(0.001));
}

TEST_CASE("A frame rate nothing could play at falls back rather than stalling") {
    CHECK(Document::FrameRate(FixedPoint(0)) == Catch::Approx(Document::kFallbackFrameRate));
    CHECK(Document::FrameRate(FixedPoint(-60 * 1024)) ==
          Catch::Approx(Document::kFallbackFrameRate));
    CHECK(Document::FrameRate(Floating(0.0F)) == Catch::Approx(Document::kFallbackFrameRate));
    CHECK(Document::FrameRate(Floating(100000.0F)) == Catch::Approx(Document::kFallbackFrameRate));
    CHECK(Document::FrameRate(Floating(std::bit_cast<float>(uint32_t{0x7FC00000}))) ==
          Catch::Approx(Document::kFallbackFrameRate));
}

TEST_CASE("The fixed point flag decides how the same bytes read") {
    const auto as_fixed = std::bit_cast<uint32_t>(30 * 1024);
    AfpAnimation::Animation fixed;
    fixed.flags = 0x2;
    fixed.fps = as_fixed;
    AfpAnimation::Animation loose;
    loose.flags = 0;
    loose.fps = as_fixed;
    CHECK(Document::FrameRate(fixed) == Catch::Approx(30.0));
    CHECK(Document::FrameRate(loose) == Catch::Approx(Document::kFallbackFrameRate));

    const auto as_float = std::bit_cast<uint32_t>(30.0F);
    AfpAnimation::Animation stored_float;
    stored_float.flags = 0;
    stored_float.fps = as_float;
    AfpAnimation::Animation read_as_fixed;
    read_as_fixed.flags = 0x2;
    read_as_fixed.fps = as_float;
    CHECK(Document::FrameRate(stored_float) == Catch::Approx(30.0));
    CHECK(Document::FrameRate(read_as_fixed) == Catch::Approx(Document::kFallbackFrameRate));
}

TEST_CASE("A frame interval is whole milliseconds and never zero") {
    CHECK(Document::FrameIntervalMs(60.0) == 17);
    CHECK(Document::FrameIntervalMs(30.0) == 33);
    CHECK(Document::FrameIntervalMs(1.0) == 1000);
    CHECK(Document::FrameIntervalMs(Document::kFastestFrameRate) >= 1);
    CHECK(Document::FrameIntervalMs(0.0) == 1000);
}

TEST_CASE("Playback walks to the next frame") {
    const Document::Step step = Document::Advance(Document::Playback{.frame_count = 4}, 1);
    CHECK(step.frame == 2);
    CHECK(step.playing);
}

TEST_CASE("Playback wraps at the end when it loops") {
    const Document::Step step =
        Document::Advance(Document::Playback{.frame_count = 4, .looping = true}, 3);
    CHECK(step.frame == 0);
    CHECK(step.playing);
}

TEST_CASE("Playback stops on the last frame when it does not loop") {
    const Document::Step step =
        Document::Advance(Document::Playback{.frame_count = 4, .looping = false}, 3);
    CHECK(step.frame == 3);
    CHECK_FALSE(step.playing);
}

TEST_CASE("Playback past the end lands on the last frame rather than running away") {
    const Document::Step step =
        Document::Advance(Document::Playback{.frame_count = 4, .looping = false}, 9);
    CHECK(step.frame == 3);
    CHECK_FALSE(step.playing);
}

TEST_CASE("An animation with no frames plays nothing") {
    const Document::Step step = Document::Advance(Document::Playback{.frame_count = 0}, 0);
    CHECK(step.frame == 0);
    CHECK_FALSE(step.playing);
}

TEST_CASE("An animation of one frame loops on itself without stopping") {
    const Document::Step step =
        Document::Advance(Document::Playback{.frame_count = 1, .looping = true}, 0);
    CHECK(step.frame == 0);
    CHECK(step.playing);
}
