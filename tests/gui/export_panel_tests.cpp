#include "gui_test_harness.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "media/media_format.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "state/live_controls.h"
#include "state/telemetry.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <variant>

namespace {

void ReadyForExport(const char* ifs) {
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene(ifs, 0, 300);
    App::Global().SetRenderSize(1280, 720);
}

void OpenModal(ImGuiTestContext* ctx) {
    ctx->SetRef("##main");
    GuiTest::FocusChild(ctx, "topbar");
    ctx->ItemClick("\xEE\xA2\x98"
                   "  Export...");
    ctx->SetRef("Export");
}

void OpenAdvanced(ImGuiTestContext* ctx) {
    ImGuiTestItemInfo const info = ctx->ItemInfo("Advanced");
    if ((info.StatusFlags & ImGuiItemStatusFlags_Opened) == 0) ctx->ItemClick("Advanced");
}

const App::Cmd::StartExport* TakeStart(std::optional<App::Command>& slot) {
    slot = App::Global().TakeCommand();
    if (!slot.has_value()) return nullptr;
    return std::get_if<App::Cmd::StartExport>(&*slot);
}

}

TEST_CASE("export modal opens with a stem derived from the active IFS", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_stem.ifs");
    App::Status status = App::Global().GetStatus();
    status.playing_animation = "bg_main";
    App::Global().SetStatus(status);

    ImGuiTest* test = harness.NewTest("export_stem");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        IM_CHECK(ctx->ItemExists("##exp_stem"));
        GuiTest::ComboPick(ctx, "##exp_fmt", "AVIF (image, alpha)");
        ctx->ItemClick("Start export");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* start = TakeStart(slot);
    REQUIRE(start != nullptr);
    CHECK(start->req.output_path ==
          MediaSink::DeriveExportStem("bg_stem.ifs", "bg_main") + ".avif");
}

TEST_CASE("export modal filename and format drive the output path", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_fmt.ifs");

    ImGuiTest* test = harness.NewTest("export_format");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        ctx->ItemInputValue("##exp_stem", "clip_out");
        GuiTest::ComboPick(ctx, "##exp_fmt", "WebM VP9 (video, alpha, software)");
        ctx->ItemClick("Start export");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* start = TakeStart(slot);
    REQUIRE(start != nullptr);
    CHECK(start->req.output_path == "clip_out.webm");
    CHECK(start->req.format == static_cast<int>(MediaSink::Format::WebM_VP9));
}

TEST_CASE("export modal PNG sequence writes a directory path", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_png.ifs");

    ImGuiTest* test = harness.NewTest("export_png_dir");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        ctx->ItemInputValue("##exp_stem", "frames_out");
        GuiTest::ComboPick(ctx, "##exp_fmt", "PNG sequence (folder of frames, lossless)");
        ctx->ItemClick("Start export");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* start = TakeStart(slot);
    REQUIRE(start != nullptr);
    CHECK(start->req.output_path == "frames_out");
}

TEST_CASE("export modal fps and quality reach the request", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_fps.ifs");

    ImGuiTest* test = harness.NewTest("export_fps_quality");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        GuiTest::ComboPick(ctx, "##exp_fmt", "AVIF (image, alpha)");
        ctx->ItemInputValue("fps##exp_fps", 24);
        ctx->ItemInputValue("##exp_q", 85);
        ctx->ItemClick("Start export");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* start = TakeStart(slot);
    REQUIRE(start != nullptr);
    CHECK(start->req.fps == 24);
    CHECK(start->req.quality == 85);
}

TEST_CASE("export modal resolution preset pins the output size", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_res.ifs");

    ImGuiTest* test = harness.NewTest("export_res_preset");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        GuiTest::ComboPick(ctx, "##exp_sz", "1920x1080");
        ctx->ItemClick("Start export");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* start = TakeStart(slot);
    REQUIRE(start != nullptr);
    CHECK(start->req.width == 1920);
    CHECK(start->req.height == 1080);
}

TEST_CASE("export modal native resolution leaves the size unset", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_native.ifs");

    ImGuiTest* test = harness.NewTest("export_res_native");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        GuiTest::ComboPick(ctx, "##exp_sz", "Native (1280x720)");
        ctx->ItemClick("Start export");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* start = TakeStart(slot);
    REQUIRE(start != nullptr);
    CHECK(start->req.width == 0);
    CHECK(start->req.height == 0);
}

TEST_CASE("export modal custom resolution inputs clamp", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_custom.ifs");

    ImGuiTest* test = harness.NewTest("export_res_custom");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        ctx->ItemInputValue("##exp_w", 999);
        ctx->ItemInputValue("##exp_h", 10);
        ctx->ItemClick("Start export");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* start = TakeStart(slot);
    REQUIRE(start != nullptr);
    CHECK(start->req.width == 999);
    CHECK(start->req.height == 64);
}

TEST_CASE("export modal scale button halves the current size", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_scale.ifs");

    ImGuiTest* test = harness.NewTest("export_scale_half");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        GuiTest::ComboPick(ctx, "##exp_sz", "1920x1080");
        ctx->ItemClick("x0.5 (960x540)##exp_scl_x0.5##exp_scl");
        ctx->ItemClick("Start export");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* start = TakeStart(slot);
    REQUIRE(start != nullptr);
    CHECK(start->req.width == 960);
    CHECK(start->req.height == 540);
}

TEST_CASE("export modal background toggle and colour reach the request", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_bgcol.ifs");

    ImGuiTest* test = harness.NewTest("export_bg");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        ctx->ItemUncheck("Transparent bg");
        ctx->ItemClick("Start export");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* start = TakeStart(slot);
    REQUIRE(start != nullptr);
    CHECK_FALSE(start->req.bg_transparent);

    ImGuiTest* back = harness.NewTest("export_bg_back");
    back->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        ctx->ItemCheck("Transparent bg");
        ctx->ItemClick("Start export");
    };
    harness.Run(back);

    std::optional<App::Command> slot2;
    const auto* start2 = TakeStart(slot2);
    REQUIRE(start2 != nullptr);
    CHECK(start2->req.bg_transparent);
}

TEST_CASE("export modal keyframe interval is offered only for video formats", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_keyint.ifs");

    ImGuiTest* test = harness.NewTest("export_keyint");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        OpenAdvanced(ctx);
        GuiTest::ComboPick(ctx, "##exp_fmt", "PNG sequence (folder of frames, lossless)");
        IM_CHECK(ctx->ItemExists("keyframe interval##exp_keyint") == false);
        GuiTest::ComboPick(ctx, "##exp_fmt", "WebM AV1 (video, opaque, NVENC)");
        ctx->ItemInputValue("keyframe interval##exp_keyint", 240);
        ctx->ItemClick("Start export");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* start = TakeStart(slot);
    REQUIRE(start != nullptr);
    CHECK(start->req.keyframe_interval == 240);
}

TEST_CASE("export modal frame limit is applied only while enabled", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_limit.ifs");

    ImGuiTest* on_test = harness.NewTest("export_limit_on");
    on_test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        OpenAdvanced(ctx);
        ctx->ItemCheck("Limit frames##exp_limit");
        ctx->ItemInputValue("frames##exp_maxf", 90);
        ctx->ItemClick("Start export");
    };
    harness.Run(on_test);

    std::optional<App::Command> slot;
    const auto* start = TakeStart(slot);
    REQUIRE(start != nullptr);
    CHECK(start->req.max_frames == 90);

    ImGuiTest* off_test = harness.NewTest("export_limit_off");
    off_test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        OpenAdvanced(ctx);
        ctx->ItemUncheck("Limit frames##exp_limit");
        ctx->ItemClick("Start export");
    };
    harness.Run(off_test);

    std::optional<App::Command> slot2;
    const auto* start2 = TakeStart(slot2);
    REQUIRE(start2 != nullptr);
    CHECK(start2->req.max_frames == 0);
}

TEST_CASE("export modal loop count and blend seam reach the request", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_loop.ifs");

    ImGuiTest* test = harness.NewTest("export_loop_blend");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        OpenAdvanced(ctx);
        ctx->ItemInputValue("Continuous loop count##exp_loops", 3);
        ctx->ItemCheck("Blend loop seam##exp_blend");
        ctx->ItemInputValue("Blend frames##exp_blendN", 20);
        ctx->ItemClick("Start export");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* start = TakeStart(slot);
    REQUIRE(start != nullptr);
    CHECK(start->req.loop_count == 3);
    CHECK(start->req.blend_loop);
    CHECK(start->req.blend_frames == 20);
}

TEST_CASE("export modal blend frame count hides while the seam is off", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_noblend.ifs");

    ImGuiTest* test = harness.NewTest("export_blend_hidden");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        OpenAdvanced(ctx);
        ctx->ItemUncheck("Blend loop seam##exp_blend");
        ctx->Yield(2);
        IM_CHECK(ctx->ItemExists("Blend frames##exp_blendN") == false);
    };
    harness.Run(test);
}

TEST_CASE("export modal crop inputs reach the request", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_crop.ifs");

    ImGuiTest* test = harness.NewTest("export_crop");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        OpenAdvanced(ctx);
        ctx->ItemInputValue("x##crop_x", 10);
        ctx->ItemInputValue("y##crop_y", 20);
        ctx->ItemInputValue("w##crop_w", 300);
        ctx->ItemInputValue("h##crop_h", 400);
        ctx->ItemClick("Start export");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* start = TakeStart(slot);
    REQUIRE(start != nullptr);
    CHECK(start->req.crop_x == 10);
    CHECK(start->req.crop_y == 20);
    CHECK(start->req.crop_w == 300);
    CHECK(start->req.crop_h == 400);
}

TEST_CASE("export modal crop Clear resets the rect", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_cropclear.ifs");
    App::Global().SetCropRect({.x = 5, .y = 6, .w = 7, .h = 8});

    ImGuiTest* test = harness.NewTest("export_crop_clear");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        OpenAdvanced(ctx);
        ctx->ItemClick("Clear##crop_clear");
    };
    harness.Run(test);

    App::CropRect const rect = App::Global().GetCropRect();
    CHECK(rect.x == 0);
    CHECK(rect.w == 0);
    CHECK_FALSE(App::Global().GetCropPickMode());
}

TEST_CASE("export modal Pick region arms crop mode, closes, then reopens", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_croppick.ifs");

    ImGuiTest* arm = harness.NewTest("export_crop_arm");
    arm->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        OpenAdvanced(ctx);
        ctx->ItemClick("Pick region##crop_pick");
        ctx->Yield(2);
        IM_CHECK(ctx->WindowInfo("//Export", ImGuiTestOpFlags_NoError).Window == nullptr);
    };
    harness.Run(arm);
    REQUIRE(App::Global().GetCropPickMode());

    App::Global().SetCropPickMode(false);

    ImGuiTest* reopen = harness.NewTest("export_crop_reopen");
    reopen->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->Yield(3);
        IM_CHECK(ctx->WindowInfo("//Export").Window != nullptr);
        ctx->SetRef("Export");
        ctx->ItemClick("Close");
    };
    harness.Run(reopen);
}

TEST_CASE("export modal Picking button disarms crop mode", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_cropcancel.ifs");
    App::Global().SetCropPickMode(true);

    ImGuiTest* test = harness.NewTest("export_crop_disarm");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        OpenAdvanced(ctx);
        ctx->ItemClick("Picking...##crop_pick");
    };
    harness.Run(test);

    CHECK_FALSE(App::Global().GetCropPickMode());
}

TEST_CASE("export modal disables the colour picker while the bg stays transparent",
          "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_colorgate.ifs");

    ImGuiTest* test = harness.NewTest("export_color_gate");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        ctx->ItemCheck("Transparent bg");
        ctx->Yield(2);
        IM_CHECK((ctx->ItemInfo("##exp_bg_color/##ColorButton").ItemFlags &
                  ImGuiItemFlags_Disabled) != 0);
        ctx->ItemUncheck("Transparent bg");
        ctx->Yield(2);
        IM_CHECK((ctx->ItemInfo("##exp_bg_color/##ColorButton").ItemFlags &
                  ImGuiItemFlags_Disabled) == 0);
        ctx->ItemCheck("Transparent bg");
    };
    harness.Run(test);
}

TEST_CASE("export modal disables hardware encode for software-only formats", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_hwgate.ifs");

    ImGuiTest* test = harness.NewTest("export_hw_gate");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        GuiTest::ComboPick(ctx, "##exp_fmt", "PNG sequence (folder of frames, lossless)");
        IM_CHECK((ctx->ItemInfo("HW accel##exp_hw").ItemFlags & ImGuiItemFlags_Disabled) != 0);
        ctx->ItemClick("Start export");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* start = TakeStart(slot);
    REQUIRE(start != nullptr);
    CHECK_FALSE(start->req.prefer_hardware);
}

TEST_CASE("export modal Close dismisses without posting a command", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_close.ifs");

    ImGuiTest* test = harness.NewTest("export_close");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        ctx->ItemClick("Close");
        ctx->Yield(2);
        IM_CHECK(ctx->WindowInfo("//Export", ImGuiTestOpFlags_NoError).Window == nullptr);
    };
    harness.Run(test);

    CHECK_FALSE(App::Global().TakeCommand().has_value());
}

TEST_CASE("export modal offers Cancel while a capture runs", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_cancel.ifs");
    App::ExportState ex;
    ex.phase = App::ExportPhase::Capturing;
    ex.frames_captured = 42;
    App::Global().SetExport(ex);

    ImGuiTest* test = harness.NewTest("export_cancel");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        IM_CHECK(ctx->ItemExists("Start export") == false);
        ctx->ItemClick("Cancel");
    };
    harness.Run(test);

    std::optional<App::Command> const cmd = App::Global().TakeCommand();
    REQUIRE(cmd.has_value());
    CHECK(std::holds_alternative<App::Cmd::CancelExport>(*cmd));
}

TEST_CASE("export modal reveals the finished file", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_done.ifs");
    App::ExportState ex;
    ex.phase = App::ExportPhase::Done;
    ex.output_path = "C:/out/done.avif";
    App::Global().SetExport(ex);

    ImGuiTest* test = harness.NewTest("export_reveal");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        ctx->ItemClick("Open folder");
    };
    harness.Run(test);

    CHECK(GuiTest::TakeRevealedPath() == "C:/out/done.avif");
}

TEST_CASE("export modal closes on Escape", "[gui][export]") {
    GuiTest::Harness harness;
    ReadyForExport("bg_escape.ifs");

    ImGuiTest* test = harness.NewTest("export_escape_close");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenModal(ctx);
        IM_CHECK(ctx->WindowInfo("//Export").Window != nullptr);
        ctx->KeyPress(ImGuiKey_Escape);
        ctx->Yield(3);
        IM_CHECK(ctx->WindowInfo("//Export", ImGuiTestOpFlags_NoError).Window == nullptr);
    };
    harness.Run(test);

    CHECK_FALSE(App::Global().TakeCommand().has_value());
}
