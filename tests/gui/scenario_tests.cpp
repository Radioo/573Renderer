#include "gui_gpu.h"
#include "gui_mock_assets.h"
#include "gui_test_harness.h"

#include "backend/afp_commands.h"
#include "gui_icons.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "media/media_format.h"
#include "qpro/qpro_model.h"
#include "qpro/qpro_scan.h"
#include "scene3d/scene3d_host.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "state/ifs_catalog.h"
#include "state/live_controls.h"
#include "state/telemetry.h"
#include "warp_device.h"
#include "window.h"

#include <catch2/catch_test_macros.hpp>

#include <windows.h>

#include <any>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

template <typename T> const T* TakeAfp(std::optional<App::Command>& slot) {
    slot = App::Global().TakeCommand();
    if (!slot.has_value()) return nullptr;
    const auto* wrapped = std::get_if<App::Cmd::BackendCommand>(&*slot);
    if (wrapped == nullptr) return nullptr;
    const auto* payload = std::any_cast<AfpCmd::Any>(&wrapped->payload);
    if (payload == nullptr) return nullptr;
    return std::get_if<T>(payload);
}

void SeedCatalog() {
    std::vector<App::State::IfsEntry> entries;
    entries.push_back({.name = "bg_intro.ifs", .full_path = "bg_intro.ifs", .from_arc = false});
    entries.push_back({.name = "bg_stage.ifs", .full_path = "bg_stage.ifs", .from_arc = false});
    App::Global().SetAvailableIfs(std::move(entries));
}

}

TEST_CASE("scenario: first run picks a game, a profile and a resolution then boots",
          "[gui][scenario]") {
    GuiTest::Harness harness;
    GuiTest::SetBrowseResult("D:/konami/ddr");

    ImGuiTest* test = harness.NewTest("scenario_first_run");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ctx->ItemClick("Browse...");
        ctx->ComboClick("##game_profile/DDR World (MDX)");
        ctx->ComboClick("##render_preset/1280x720 (DDR)");
        ctx->ItemClick("60##fps_60");
        ctx->ItemClick("Load");
    };
    harness.Run(test);

    auto& state = App::Global();
    CHECK(state.GameDir() == "D:/konami/ddr");
    CHECK(state.GetGameProfileSlug() == "ddrworld");
    CHECK(state.GetRenderFps() == 60);

    std::optional<App::Command> const cmd = state.TakeCommand();
    REQUIRE(cmd.has_value());
    const auto* boot = std::get_if<App::Cmd::BootGame>(&*cmd);
    REQUIRE(boot != nullptr);
    CHECK(boot->game_dir == "D:/konami/ddr");
    CHECK(boot->render_width == 1280);
    CHECK(boot->render_height == 720);
}

TEST_CASE("scenario: browse, load an IFS, then play one of its layers", "[gui][scenario]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    SeedCatalog();

    ImGuiTest* pick = harness.NewTest("scenario_browse_pick");
    pick->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_left");
        ctx->ItemInputValue("##ifsfilter", "stage");
        GuiTest::FocusChild(ctx, "ifs_scroll");
        ctx->ItemClick("bg_stage.ifs/bg_stage.ifs");
    };
    harness.Run(pick);

    std::optional<App::Command> const load = App::Global().TakeCommand();
    REQUIRE(load.has_value());
    const auto* content = std::get_if<App::Cmd::LoadContent>(&*load);
    REQUIRE(content != nullptr);
    CHECK(content->path == "bg_stage.ifs");

    GuiTest::LoadScene("bg_stage.ifs", 0, 240);
    {
        auto& cfg = App::Global().MutConfig("bg_stage.ifs");
        cfg.filename = "bg_stage.ifs";
        cfg.anim_names = {"stage_main", "stage_alt"};
    }

    ImGuiTest* play = harness.NewTest("scenario_play_layer");
    play->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_left");
        ctx->ItemInputValue("##ifsfilter", "");
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_center/scene_scroll");
        ctx->ItemDoubleClick("$$1/##layer");
    };
    harness.Run(play);

    std::optional<App::Command> slot;
    const auto* sw = TakeAfp<AfpCmd::SwitchAnimation>(slot);
    REQUIRE(sw != nullptr);
    CHECK(sw->name == "stage_alt");
}

TEST_CASE("scenario: pause, step, jump to a label, then resume", "[gui][scenario]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_scrub.ifs", 100, 300);
    App::Status status = App::Global().GetStatus();
    status.labels.push_back({.name = "drop", .frame = 200});
    App::Global().SetStatus(status);

    ImGuiTest* test = harness.NewTest("scenario_scrub");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/##timeline_dock");
        ctx->ItemClick(ICON_PAUSE);
        ctx->ItemClick(ICON_STEP_FWD);
        ctx->ComboClick("##tl_labels/drop   (frame 200)##lbl0");
        ctx->ItemClick(ICON_PLAY);
    };
    harness.Run(test);

    std::optional<App::Command> s1;
    const auto* paused = TakeAfp<AfpCmd::SetPaused>(s1);
    REQUIRE(paused != nullptr);
    CHECK(paused->paused);

    std::optional<App::Command> s2;
    const auto* seek = TakeAfp<AfpCmd::SeekFrame>(s2);
    REQUIRE(seek != nullptr);
    CHECK(seek->frame == 101);

    std::optional<App::Command> s3;
    const auto* jump = TakeAfp<AfpCmd::GotoLabel>(s3);
    REQUIRE(jump != nullptr);
    CHECK(jump->name == "drop");

    std::optional<App::Command> s4;
    const auto* resumed = TakeAfp<AfpCmd::SetPaused>(s4);
    REQUIRE(resumed != nullptr);
    CHECK_FALSE(resumed->paused);
}

TEST_CASE("scenario: configure an export from the keyboard shortcut and start it",
          "[gui][scenario]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_export.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("scenario_export");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_E);
        ctx->Yield(3);
        ctx->SetRef("Export");
        ctx->ItemInputValue("##exp_stem", "stage_clip");
        GuiTest::ComboPick(ctx, "##exp_fmt", "WebM VP9 (video, alpha, software)");
        ctx->ItemInputValue("fps##exp_fps", 30);
        ctx->ItemInputValue("##exp_q", 90);
        GuiTest::ComboPick(ctx, "##exp_sz", "1280x720");
        ImGuiTestItemInfo const adv = ctx->ItemInfo("Advanced");
        if ((adv.StatusFlags & ImGuiItemStatusFlags_Opened) == 0) ctx->ItemClick("Advanced");
        ctx->ItemCheck("Limit frames##exp_limit");
        ctx->ItemInputValue("frames##exp_maxf", 150);
        ctx->ItemInputValue("Continuous loop count##exp_loops", 2);
        ctx->ItemClick("Start export");
    };
    harness.Run(test);

    std::optional<App::Command> const cmd = App::Global().TakeCommand();
    REQUIRE(cmd.has_value());
    const auto* start = std::get_if<App::Cmd::StartExport>(&*cmd);
    REQUIRE(start != nullptr);
    CHECK(start->req.output_path == "stage_clip.webm");
    CHECK(start->req.format == static_cast<int>(MediaSink::Format::WebM_VP9));
    CHECK(start->req.fps == 30);
    CHECK(start->req.quality == 90);
    CHECK(start->req.width == 1280);
    CHECK(start->req.height == 720);
    CHECK(start->req.max_frames == 150);
    CHECK(start->req.loop_count == 2);
}

TEST_CASE("scenario: a running export can be cancelled from the status strip", "[gui][scenario]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_cancel_flow.ifs", 0, 300);
    App::ExportState ex;
    ex.phase = App::ExportPhase::Capturing;
    ex.frames_captured = 30;
    App::Global().SetExport(ex);

    ImGuiTest* test = harness.NewTest("scenario_cancel_export");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "status_strip");
        ctx->ItemClick("capturing 30");
        ctx->Yield(3);
        ctx->SetRef("Export");
        ctx->ItemClick("Cancel");
    };
    harness.Run(test);

    std::optional<App::Command> const cmd = App::Global().TakeCommand();
    REQUIRE(cmd.has_value());
    CHECK(std::holds_alternative<App::Cmd::CancelExport>(*cmd));
}

TEST_CASE("scenario: arm a crop in the modal, draw it on the render window, export it",
          "[gui][scenario]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_crop_flow.ifs", 0, 300);
    App::Global().SetCropRect({});

    ImGuiTest* arm = harness.NewTest("scenario_crop_arm");
    arm->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        ctx->ItemClick(ICON_EXPORT "  Export...");
        ctx->SetRef("Export");
        ImGuiTestItemInfo const adv = ctx->ItemInfo("Advanced");
        if ((adv.StatusFlags & ImGuiItemStatusFlags_Opened) == 0) ctx->ItemClick("Advanced");
        ctx->ItemClick("Pick region##crop_pick");
        ctx->Yield(2);
        IM_CHECK(ctx->WindowInfo("//Export", ImGuiTestOpFlags_NoError).Window == nullptr);
    };
    harness.Run(arm);
    REQUIRE(App::Global().GetCropPickMode());

    HWND render = AppWindow::Create(640, 480);
    REQUIRE(render != nullptr);
    AppWindow::SetRenderRtSize(640, 480);
    SendMessageW(render, WM_LBUTTONDOWN, 0, MAKELPARAM(80, 60));
    SendMessageW(render, WM_MOUSEMOVE, 0, MAKELPARAM(300, 240));
    SendMessageW(render, WM_LBUTTONUP, 0, MAKELPARAM(300, 240));
    DestroyWindow(render);
    for (MSG msg; PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != 0;) {
    }

    CHECK_FALSE(App::Global().GetCropPickMode());
    App::CropRect const drawn = App::Global().GetCropRect();
    REQUIRE(drawn.w > 0);
    REQUIRE(drawn.h > 0);

    ImGuiTest* finish = harness.NewTest("scenario_crop_export");
    finish->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->Yield(3);
        IM_CHECK(ctx->WindowInfo("//Export").Window != nullptr);
        ctx->SetRef("Export");
        ctx->ItemClick("Start export");
    };
    harness.Run(finish);

    std::optional<App::Command> const cmd = App::Global().TakeCommand();
    REQUIRE(cmd.has_value());
    const auto* start = std::get_if<App::Cmd::StartExport>(&*cmd);
    REQUIRE(start != nullptr);
    CHECK(start->req.crop_w == drawn.w);
    CHECK(start->req.crop_h == drawn.h);

    App::Global().SetCropRect({});
}

TEST_CASE("scenario: qpro scan, narrow to one date group, then extract", "[gui][scenario]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "iidx33");
    GuiTest::SetBrowseResult("D:/qpro_scenario");
    std::vector<QproExtract::ScanPart> parts;
    parts.push_back({.cat = static_cast<int>(QproModel::Category::Body),
                     .idx = 0,
                     .label = "body_0",
                     .ifs = "a.ifs",
                     .date = "2026-03-02",
                     .exists = true});
    parts.push_back({.cat = static_cast<int>(QproModel::Category::Body),
                     .idx = 1,
                     .label = "body_1",
                     .ifs = "b.ifs",
                     .date = "2024-01-05",
                     .exists = true});
    QproExtract::PublishScanResult(std::move(parts), {});

    ImGuiTest* test = harness.NewTest("scenario_qpro");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        ctx->ItemClick("qpro");
        ctx->Yield(2);
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view");
        ctx->ItemClick("All");
        GuiTest::FocusChild(ctx, "qpro_parts");
        ctx->ItemUncheck("$$1/##grp");
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view");
        ctx->ItemCheck("Body");
        ctx->ItemInputValue("Output fps", 30);
        ctx->ItemClick("Choose output folder + extract...");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* extract = TakeAfp<AfpCmd::QproStartExtract>(slot);
    REQUIRE(extract != nullptr);
    CHECK(extract->out_dir == "D:/qpro_scenario");
    CHECK(extract->fps == 30);
    CHECK(extract->parts.body);
    REQUIRE(extract->part_sel.sel[0].size() == 2);
    CHECK(extract->part_sel.sel[0][0] == 1);
    CHECK(extract->part_sel.sel[0][1] == 0);

    QproExtract::PublishScanResult({}, {});
}

TEST_CASE("scenario: live overrides can be stacked then reset in one click", "[gui][scenario]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_overrides.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("scenario_overrides");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemClick("##inspector_tabs/Render");
        ctx->Yield(2);
        ctx->ItemClick("##inspector_tabs/Render/##live_cont/$$2/ON");
        ctx->ItemClick("##inspector_tabs/Render/##bg_color/$$3/red");
        ctx->ItemCheck("##inspector_tabs/Render/Filter (F7)");
        ctx->ItemInputValue("##inspector_tabs/Render/##live_trim", 90);
    };
    harness.Run(test);

    auto const staged = App::Global().GetLiveOverrides();
    CHECK(staged.continuous_loop_mode == 1);
    CHECK(staged.bg_color_index == 2);
    CHECK(staged.filter_enabled);
    CHECK(staged.trim_frames == 90);

    ImGuiTest* reset = harness.NewTest("scenario_overrides_reset");
    reset->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemClick("##inspector_tabs/Render/Reset live overrides");
    };
    harness.Run(reset);

    auto const cleared = App::Global().GetLiveOverrides();
    CHECK(cleared.continuous_loop_mode == 0);
    CHECK(cleared.bg_color_index == -1);
    CHECK_FALSE(cleared.filter_enabled);
    CHECK(cleared.trim_frames == 0);
}

TEST_CASE("scenario: loading and unloading a 3D scene adds and removes its tab",
          "[gui][scenario][live]") {
    GuiTest::WarpGpu const gpu;
    if (!gpu.ok()) SKIP("D3D9On12/WARP unavailable: " << WarpD3D9::LastError());
    GuiTest::TempAssetDir const assets("scenario_scene3d");
    GuiTest::WriteMock3dScene(assets.path(), true);

    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_ddr", "ddrworld");

    ImGuiTest* before = harness.NewTest("scenario_scene3d_before");
    before->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        IM_CHECK(ctx->ItemExists("##inspector_tabs/3D scene") == false);
    };
    harness.Run(before);

    REQUIRE(Scene3dHost::Load(assets.path()));

    ImGuiTest* drive = harness.NewTest("scenario_scene3d_drive");
    drive->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        IM_CHECK(ctx->ItemExists("##inspector_tabs/3D scene"));
        ctx->ItemClick("##inspector_tabs/3D scene");
        ctx->Yield(2);
        ctx->ItemClick("##inspector_tabs/3D scene/Pause##s3d");
        ctx->ItemInputValue("##inspector_tabs/3D scene/##s3dtime", 120.0F);
        ctx->ItemClick("##inspector_tabs/3D scene/$$0/##vis");
    };
    harness.Run(drive);

    CHECK(Scene3dHost::GetStatus().paused);
    CHECK(Scene3dHost::GetStatus().time == 120.0F);
    CHECK_FALSE(Scene3dHost::ListModels()[0].visible);

    Scene3dHost::Unload();

    ImGuiTest* after = harness.NewTest("scenario_scene3d_after");
    after->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        IM_CHECK(ctx->ItemExists("##inspector_tabs/3D scene") == false);
    };
    harness.Run(after);
}
