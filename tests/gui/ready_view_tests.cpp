#include "gui_test_harness.h"

#include "backend/afp_commands.h"
#include "gui_icons.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "state/boot_lifecycle.h"
#include "state/telemetry.h"

#include <catch2/catch_test_macros.hpp>

#include <any>
#include <cstdint>
#include <optional>
#include <variant>

namespace {

constexpr const char* kDock = "main_view/##timeline_dock";
constexpr const char* kStatusStrip = "status_strip";

void MakeSceneReady(uint32_t cur, uint32_t total) {
    auto& state = App::Global();
    state.SetBootState(App::BootState::Ready);
    state.SetActiveBackendId("afp_modern");
    state.SetGameProfileSlug("ddrworld");

    App::Status status;
    status.scene_loaded = true;
    status.current_ifs_path = "bg_0001.ifs";
    state.SetStatus(status);

    App::State::LiveState live;
    live.mc_cur = cur;
    live.mc_total = total;
    live.have_mc_playhead = true;
    state.SetLiveState(live);
}

void AddLabels() {
    auto& state = App::Global();
    App::Status status = state.GetStatus();
    status.labels.push_back({.name = "intro", .frame = 0});
    status.labels.push_back({.name = "chorus", .frame = 120});
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

TEST_CASE("timeline shows a hint until a scene is loaded", "[gui][timeline]") {
    GuiTest::Harness harness;
    App::Global().SetBootState(App::BootState::Ready);
    App::Global().SetActiveBackendId("afp_modern");

    ImGuiTest* test = harness.NewTest("timeline_no_scene");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, kDock);
        IM_CHECK(ctx->ItemExists(ICON_PAUSE) == false);
    };
    harness.Run(test);

    CHECK_FALSE(App::Global().TakeCommand().has_value());
}

TEST_CASE("timeline play button posts SetPaused and flips the override", "[gui][timeline]") {
    GuiTest::Harness harness;
    MakeSceneReady(40, 300);

    ImGuiTest* test = harness.NewTest("timeline_pause");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, kDock);
        ctx->ItemClick(ICON_PAUSE);
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* paused = TakeAfpCommand<AfpCmd::SetPaused>(slot);
    REQUIRE(paused != nullptr);
    CHECK(paused->paused);
    CHECK(App::Global().GetLiveOverrides().paused);
}

TEST_CASE("timeline step forward seeks one frame and pauses", "[gui][timeline]") {
    GuiTest::Harness harness;
    MakeSceneReady(40, 300);

    ImGuiTest* test = harness.NewTest("timeline_step_fwd");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, kDock);
        ctx->ItemClick(ICON_STEP_FWD);
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* seek = TakeAfpCommand<AfpCmd::SeekFrame>(slot);
    REQUIRE(seek != nullptr);
    CHECK(seek->frame == 41);
    CHECK(App::Global().GetLiveOverrides().paused);
}

TEST_CASE("timeline step back wraps around the master timeline", "[gui][timeline]") {
    GuiTest::Harness harness;
    MakeSceneReady(0, 300);

    ImGuiTest* test = harness.NewTest("timeline_step_back_wrap");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, kDock);
        ctx->ItemClick(ICON_STEP_BACK);
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* seek = TakeAfpCommand<AfpCmd::SeekFrame>(slot);
    REQUIRE(seek != nullptr);
    CHECK(seek->frame == 299);
}

TEST_CASE("timeline jump forward advances 100 frames", "[gui][timeline]") {
    GuiTest::Harness harness;
    MakeSceneReady(10, 300);

    ImGuiTest* test = harness.NewTest("timeline_jump_fwd");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, kDock);
        ctx->ItemClick(ICON_JUMP_FWD);
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* seek = TakeAfpCommand<AfpCmd::SeekFrame>(slot);
    REQUIRE(seek != nullptr);
    CHECK(seek->frame == 110);
}

TEST_CASE("timeline label combo posts GotoLabel", "[gui][timeline]") {
    GuiTest::Harness harness;
    MakeSceneReady(0, 300);
    AddLabels();

    ImGuiTest* test = harness.NewTest("timeline_label_combo");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, kDock);
        ctx->ComboClick("##tl_labels/chorus   (frame 120)##lbl1");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* goto_label = TakeAfpCommand<AfpCmd::GotoLabel>(slot);
    REQUIRE(goto_label != nullptr);
    CHECK(goto_label->name == "chorus");
}

TEST_CASE("status strip Open folder reveals the finished export", "[gui][status]") {
    GuiTest::Harness harness;
    MakeSceneReady(0, 300);
    App::ExportState ex;
    ex.phase = App::ExportPhase::Done;
    ex.output_path = "C:/out/bg_0001.webm";
    App::Global().SetExport(ex);

    ImGuiTest* test = harness.NewTest("status_open_folder");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, kStatusStrip);
        ctx->ItemClick("Open folder");
    };
    harness.Run(test);

    CHECK(GuiTest::TakeRevealedPath() == "C:/out/bg_0001.webm");
}
