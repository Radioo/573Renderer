#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "judge/interp_layout.h"
#include "judge/interp_tracks.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr float kFps = 60.0F;
constexpr float kStep = 1.0F / 60.0F;
constexpr std::uintptr_t kRoot = 0x1000;
constexpr std::uintptr_t kChild = 0x2000;

AfpInterp::Sample At(std::uintptr_t object, float x, std::uintptr_t parent = kRoot) {
    AfpInterp::Sample sample;
    sample.object = object;
    sample.parent = parent;
    sample.pose.character = 5;
    sample.pose.matrix = {1.0F, 0.0F, 0.0F, 1.0F, x, 0.0F};
    return sample;
}

void Advance(AfpInterp::LayerTracks& tracks, std::vector<AfpInterp::Sample> before,
             std::vector<AfpInterp::Sample> after, int steps) {
    tracks.BeginAdvance(before);
    tracks.EndAdvance(steps, after);
}

float DrawnX(const AfpInterp::LayerTracks& tracks, const AfpInterp::Sample& live, float alpha) {
    const auto pose = tracks.DrawPose(live.object, live.pose, alpha, AfpPose::MatrixLerp::Linear);
    return pose ? pose->matrix[4] : live.pose.matrix[4];
}

}

TEST_CASE("Frame steps count what the advance crossed, including float residue") {
    const AfpInterp::LayerTiming stepped{.fps = kFps, .accumulator = 0.0000001F};
    CHECK(AfpInterp::FrameSteps(stepped, kStep, 0.0F) == 1);
    const AfpInterp::LayerTiming half{.fps = kFps, .accumulator = kStep / 2.0F};
    CHECK(AfpInterp::FrameSteps(half, kStep / 2.0F, 0.0F) == 0);
    const AfpInterp::LayerTiming hitch{.fps = kFps, .accumulator = 0.0F};
    CHECK(AfpInterp::FrameSteps(hitch, 3.0F * kStep, 0.0F) == 3);
    const AfpInterp::LayerTiming frozen{.fps = kFps, .frozen = true, .accumulator = 0.0F};
    CHECK(AfpInterp::FrameSteps(frozen, kStep, 0.0F) == 0);
    CHECK(AfpInterp::FrameSteps(AfpInterp::LayerTiming{.fps = 0.0F}, kStep, 0.0F) == -1);
}

TEST_CASE("Alpha is the time since the last authored frame step in frames, clamped") {
    const AfpInterp::LayerTiming just_stepped{.fps = kFps, .accumulator = 0.0F};
    CHECK(AfpInterp::Alpha(just_stepped, 0.0F) == Catch::Approx(0.0F));
    CHECK(AfpInterp::Alpha(just_stepped, kStep / 2.0F) == Catch::Approx(0.5F));
    const AfpInterp::LayerTiming thirty{.fps = 30.0F, .accumulator = kStep};
    CHECK(AfpInterp::Alpha(thirty, kStep / 2.0F) == Catch::Approx(0.75F));
    CHECK(AfpInterp::Alpha(just_stepped, 1.0F) == Catch::Approx(1.0F));
    const AfpInterp::LayerTiming frozen{.fps = kFps, .frozen = true, .accumulator = 0.0F};
    CHECK(AfpInterp::Alpha(frozen, kStep / 2.0F) == Catch::Approx(0.0F));
}

TEST_CASE("One frame step blends from the pose before the advance to the pose after it") {
    AfpInterp::LayerTracks tracks;
    Advance(tracks, {At(kChild, 0.0F)}, {At(kChild, 10.0F)}, 1);
    CHECK(DrawnX(tracks, At(kChild, 10.0F), 0.5F) == Catch::Approx(5.0F));
    CHECK(tracks.MovingCount() == 1);
}

TEST_CASE("An advance with no frame step keeps blending the same pair") {
    AfpInterp::LayerTracks tracks;
    Advance(tracks, {At(kChild, 0.0F)}, {At(kChild, 10.0F)}, 1);
    Advance(tracks, {At(kChild, 10.0F)}, {At(kChild, 10.0F)}, 0);
    CHECK(DrawnX(tracks, At(kChild, 10.0F), 0.75F) == Catch::Approx(7.5F));
}

TEST_CASE("A jump of the parent clip snaps its children") {
    AfpInterp::LayerTracks tracks;
    tracks.BeginAdvance(std::vector{At(kChild, 0.0F)});
    tracks.MarkJumped(kRoot);
    tracks.EndAdvance(1, std::vector{At(kChild, 10.0F)});
    CHECK(DrawnX(tracks, At(kChild, 10.0F), 0.5F) == Catch::Approx(10.0F));
}

TEST_CASE("A jump outside an advance is not recorded") {
    AfpInterp::LayerTracks tracks;
    tracks.MarkJumped(kRoot);
    Advance(tracks, {At(kChild, 0.0F)}, {At(kChild, 10.0F)}, 1);
    CHECK(DrawnX(tracks, At(kChild, 10.0F), 0.5F) == Catch::Approx(5.0F));
}

TEST_CASE("Script controlled objects and multi-step advances snap") {
    AfpInterp::LayerTracks tracks;
    auto scripted = At(kChild, 10.0F);
    scripted.script_controlled = true;
    Advance(tracks, {At(kChild, 0.0F)}, {scripted}, 1);
    CHECK(DrawnX(tracks, scripted, 0.5F) == Catch::Approx(10.0F));
    Advance(tracks, {At(kChild, 0.0F)}, {At(kChild, 10.0F)}, 2);
    CHECK(DrawnX(tracks, At(kChild, 10.0F), 0.5F) == Catch::Approx(10.0F));
}

TEST_CASE("An object constructed during the advance at a reused address snaps") {
    AfpInterp::LayerTracks tracks;
    tracks.BeginAdvance(std::vector{At(kChild, 0.0F)});
    tracks.Forget(kChild);
    tracks.EndAdvance(1, std::vector{At(kChild, 10.0F)});
    CHECK(DrawnX(tracks, At(kChild, 10.0F), 0.5F) == Catch::Approx(10.0F));
}

TEST_CASE("A live pose that differs from the authored one is drawn as it is") {
    AfpInterp::LayerTracks tracks;
    Advance(tracks, {At(kChild, 0.0F)}, {At(kChild, 10.0F)}, 1);
    CHECK(DrawnX(tracks, At(kChild, 30.0F), 0.5F) == Catch::Approx(30.0F));
}

TEST_CASE("Objects that left the tree drop their tracks") {
    AfpInterp::LayerTracks tracks;
    Advance(tracks, {At(kChild, 0.0F)}, {At(kChild, 10.0F)}, 1);
    Advance(tracks, {At(kChild, 10.0F)}, {}, 1);
    CHECK(tracks.MovingCount() == 0);
    CHECK_FALSE(tracks.DrawPose(kChild, At(kChild, 10.0F).pose, 0.5F, AfpPose::MatrixLerp::Linear));
}

TEST_CASE("A libafp whose hook prologues differ is refused with the function named") {
    AfpInterp::Layout layout{};
    layout.layer_advance = {
        .name = "AfpLayer_Advance", .rva = 0x10, .prologue = {0x57, 0x56}, .prologue_size = 2};
    layout.layer_draw = {
        .name = "AfpLayer_DrawFrame", .rva = 0x20, .prologue = {0x55}, .prologue_size = 1};
    layout.object_init = {
        .name = "AfpObj_Init", .rva = 0x30, .prologue = {0x55}, .prologue_size = 1};
    layout.clip_goto = {
        .name = "AfpClip_GotoFrame", .rva = 0x40, .prologue = {0x55}, .prologue_size = 1};
    layout.clip_set_frame = {
        .name = "AfpClip_SetFrameDirect", .rva = 0x50, .prologue = {0x55}, .prologue_size = 1};
    std::array<std::uint8_t, 0x60> image{};
    image[0x10] = 0x57;
    image[0x11] = 0x56;
    for (const std::size_t at : {0x20U, 0x30U, 0x40U, 0x50U})
        image[at] = 0x55;
    const AfpInterp::ReadImage read = [&](std::uint32_t rva, std::size_t size, std::uint8_t* out) {
        if (rva + size > image.size()) return false;
        std::memcpy(out, image.data() + rva, size);
        return true;
    };
    CHECK(AfpInterp::PrologueMismatch(layout, read).empty());
    image[0x41] = 0x00;
    image[0x40] = 0xCC;
    CHECK(AfpInterp::PrologueMismatch(layout, read).find("AfpClip_GotoFrame") != std::string::npos);
    layout.clip_goto.rva = 0x100;
    CHECK(AfpInterp::PrologueMismatch(layout, read).find("outside") != std::string::npos);
}

TEST_CASE("A goto that moves a clip forward by exactly one frame is a step, not a jump") {
    AfpInterp::LayerTracks tracks;
    tracks.BeginAdvance(std::vector{At(kChild, 0.0F)});
    tracks.MarkGoto(kRoot, 104, 105);
    tracks.EndAdvance(1, std::vector{At(kChild, 10.0F)});
    CHECK(DrawnX(tracks, At(kChild, 10.0F), 0.5F) == Catch::Approx(5.0F));

    tracks.BeginAdvance(std::vector{At(kChild, 0.0F)});
    tracks.MarkGoto(kRoot, 104, 106);
    tracks.EndAdvance(1, std::vector{At(kChild, 10.0F)});
    CHECK(DrawnX(tracks, At(kChild, 10.0F), 0.5F) == Catch::Approx(10.0F));

    tracks.BeginAdvance(std::vector{At(kChild, 0.0F)});
    tracks.MarkGoto(kRoot, 263, 0);
    tracks.EndAdvance(1, std::vector{At(kChild, 10.0F)});
    CHECK(DrawnX(tracks, At(kChild, 10.0F), 0.5F) == Catch::Approx(10.0F));
}
