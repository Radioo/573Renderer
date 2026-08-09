#include "gui_test_harness.h"

#include "backend/afp_commands.h"
#include "gui_icons.h"
#include "imgui.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "state/telemetry.h"

#include <any>
#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <variant>

namespace {

constexpr const char* kDock = "main_view/##timeline_dock";

void ReadyWithPlayhead(const char* ifs, unsigned cur, unsigned total) {
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene(ifs, cur, total);
}

void AddLabels() {
    auto& state = App::Global();
    App::Status status = state.GetStatus();
    status.labels.push_back({.name = "intro", .frame = 0});
    status.labels.push_back({.name = "mid", .frame = 150});
    state.SetStatus(status);
}

template <typename T> const T* TakeAfpCommand(std::optional<App::Command>& slot) {
    slot = App::Global().TakeCommand();
    if (!slot.has_value()) return nullptr;
    const auto* wrapped = std::get_if<App::Cmd::BackendCommand>(&*slot);
    if (wrapped == nullptr) return nullptr;
    const auto* payload = std::any_cast<AfpCmd::Any>(&wrapped->payload);
    if (payload == nullptr) return nullptr;
    return std::get_if<T>(payload);
}

}

TEST_CASE("timeline jump back rewinds 100 frames", "[gui][timeline]") {
    GuiTest::Harness harness;
    ReadyWithPlayhead("bg_jumpback.ifs", 250, 300);

    ImGuiTest* test = harness.NewTest("tl_jump_back");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, kDock);
        ctx->ItemClick(ICON_JUMP_BACK);
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* seek = TakeAfpCommand<AfpCmd::SeekFrame>(slot);
    REQUIRE(seek != nullptr);
    CHECK(seek->frame == 150);
}

TEST_CASE("timeline play button resumes a paused stream", "[gui][timeline]") {
    GuiTest::Harness harness;
    ReadyWithPlayhead("bg_resume.ifs", 10, 300);
    App::State::LiveOverrides ov;
    ov.paused = true;
    App::Global().SetLiveOverrides(ov);

    ImGuiTest* test = harness.NewTest("tl_resume");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, kDock);
        ctx->ItemClick(ICON_PLAY);
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* paused = TakeAfpCommand<AfpCmd::SetPaused>(slot);
    REQUIRE(paused != nullptr);
    CHECK_FALSE(paused->paused);
    CHECK_FALSE(App::Global().GetLiveOverrides().paused);
}

TEST_CASE("timeline track drag seeks and pauses", "[gui][timeline]") {
    GuiTest::Harness harness;
    ReadyWithPlayhead("bg_track.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("tl_track_drag");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, kDock);
        ImGuiTestItemInfo const track = ctx->ItemInfo("##tl_track");
        IM_CHECK_NE(track.ID, 0U);
        float const mid_y = (track.RectFull.Min.y + track.RectFull.Max.y) * 0.5F;
        float const quarter_x =
            track.RectFull.Min.x + ((track.RectFull.Max.x - track.RectFull.Min.x) * 0.25F);
        ctx->MouseMove("##tl_track");
        ctx->MouseMoveToPos(ImVec2(quarter_x, mid_y));
        ctx->MouseDown(0);
        ctx->Yield(2);
        ctx->MouseUp(0);
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* seek = TakeAfpCommand<AfpCmd::SeekFrame>(slot);
    REQUIRE(seek != nullptr);
    CHECK(seek->frame > 50);
    CHECK(seek->frame < 100);
    CHECK(App::Global().GetLiveOverrides().paused);
}

TEST_CASE("timeline track ignores clicks while no master length is known", "[gui][timeline]") {
    GuiTest::Harness harness;
    ReadyWithPlayhead("bg_notrack.ifs", 0, 0);

    ImGuiTest* test = harness.NewTest("tl_track_no_total");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, kDock);
        ctx->ItemClick("##tl_track");
    };
    harness.Run(test);

    CHECK_FALSE(App::Global().TakeCommand().has_value());
}

TEST_CASE("timeline label tick on the track jumps to that label", "[gui][timeline]") {
    GuiTest::Harness harness;
    ReadyWithPlayhead("bg_tick.ifs", 0, 300);
    AddLabels();

    ImGuiTest* test = harness.NewTest("tl_label_tick");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, kDock);
        ImGuiTestItemInfo const track = ctx->ItemInfo("##tl_track");
        IM_CHECK_NE(track.ID, 0U);
        float const mid_y = (track.RectFull.Min.y + track.RectFull.Max.y) * 0.5F;
        float const tick_x =
            track.RectFull.Min.x + ((track.RectFull.Max.x - track.RectFull.Min.x) * 0.5F);
        ctx->MouseMove("##tl_track");
        ctx->MouseMoveToPos(ImVec2(tick_x, mid_y));
        ctx->MouseClick(0);
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* jump = TakeAfpCommand<AfpCmd::GotoLabel>(slot);
    REQUIRE(jump != nullptr);
    CHECK(jump->name == "mid");
}

TEST_CASE("space toggles playback", "[gui][timeline]") {
    GuiTest::Harness harness;
    ReadyWithPlayhead("bg_space.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("tl_key_space");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->KeyPress(ImGuiKey_Space);
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* paused = TakeAfpCommand<AfpCmd::SetPaused>(slot);
    REQUIRE(paused != nullptr);
    CHECK(paused->paused);
}

TEST_CASE("arrow keys step one frame", "[gui][timeline]") {
    GuiTest::Harness harness;
    ReadyWithPlayhead("bg_arrow.ifs", 100, 300);

    ImGuiTest* test = harness.NewTest("tl_key_right");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->KeyPress(ImGuiKey_RightArrow);
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* seek = TakeAfpCommand<AfpCmd::SeekFrame>(slot);
    REQUIRE(seek != nullptr);
    CHECK(seek->frame == 101);
}

TEST_CASE("shift plus arrow steps one hundred frames", "[gui][timeline]") {
    GuiTest::Harness harness;
    ReadyWithPlayhead("bg_shift.ifs", 100, 300);

    ImGuiTest* test = harness.NewTest("tl_key_shift_left");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->KeyPress(ImGuiMod_Shift | ImGuiKey_LeftArrow);
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* seek = TakeAfpCommand<AfpCmd::SeekFrame>(slot);
    REQUIRE(seek != nullptr);
    CHECK(seek->frame == 0);
}

TEST_CASE("ctrl plus E opens the export modal", "[gui][timeline]") {
    GuiTest::Harness harness;
    ReadyWithPlayhead("bg_ctrle.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("tl_key_ctrl_e");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_E);
        ctx->Yield(3);
        IM_CHECK(ctx->WindowInfo("//Export").Window != nullptr);
        ctx->SetRef("Export");
        ctx->ItemClick("Close");
    };
    harness.Run(test);
}

TEST_CASE("transport shortcuts are suppressed during a capture", "[gui][timeline]") {
    GuiTest::Harness harness;
    ReadyWithPlayhead("bg_capturing.ifs", 100, 300);
    App::ExportState ex;
    ex.phase = App::ExportPhase::Capturing;
    App::Global().SetExport(ex);

    ImGuiTest* test = harness.NewTest("tl_key_during_export");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->KeyPress(ImGuiKey_RightArrow);
        ctx->KeyPress(ImGuiKey_Space);
    };
    harness.Run(test);

    CHECK_FALSE(App::Global().TakeCommand().has_value());
}

TEST_CASE("timeline shows a hint and no transport before a scene loads", "[gui][timeline]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");

    ImGuiTest* test = harness.NewTest("tl_hint");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, kDock);
        IM_CHECK(ctx->ItemExists("##tl_track") == false);
        IM_CHECK(ctx->ItemExists(ICON_PAUSE) == false);
    };
    harness.Run(test);
}

TEST_CASE("timeline label combo is absent without labels", "[gui][timeline]") {
    GuiTest::Harness harness;
    ReadyWithPlayhead("bg_nolabels.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("tl_no_labels");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, kDock);
        IM_CHECK(ctx->ItemExists("##tl_labels") == false);
    };
    harness.Run(test);
}

TEST_CASE("timeline scrub seeks through a label tick instead of stalling", "[gui][timeline]") {
    GuiTest::Harness harness;
    ReadyWithPlayhead("bg_scrubtick.ifs", 0, 300);
    AddLabels();

    ImGuiTest* test = harness.NewTest("tl_scrub_over_tick");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, kDock);
        ImGuiTestItemInfo const track = ctx->ItemInfo("##tl_track");
        IM_CHECK_NE(track.ID, 0U);
        float const y = (track.RectFull.Min.y + track.RectFull.Max.y) * 0.5F;
        float const x0 = track.RectFull.Min.x;
        float const w = track.RectFull.Max.x - track.RectFull.Min.x;

        ctx->MouseMove("##tl_track");
        ctx->MouseMoveToPos(ImVec2(x0 + (w * 0.20F), y));
        ctx->MouseDown(0);
        ctx->MouseMoveToPos(ImVec2(x0 + (w * 0.50F), y));
        ctx->Yield(2);
        ctx->MouseUp(0);
    };
    harness.Run(test);

    int last_frame = -1;
    int seeks = 0;
    while (true) {
        std::optional<App::Command> slot;
        const auto* seek = TakeAfpCommand<AfpCmd::SeekFrame>(slot);
        if (seek == nullptr) break;
        last_frame = seek->frame;
        seeks++;
    }
    CHECK(seeks > 1);
    CHECK(last_frame > 120);
}
