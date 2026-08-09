#include "gui_test_harness.h"

#include "gui_icons.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "state/app_state.h"
#include "state/ifs_catalog.h"
#include "state/telemetry.h"

#include <catch2/catch_test_macros.hpp>

#include <initializer_list>

namespace {

void ExpectTooltips(ImGuiTestContext* ctx, std::initializer_list<const char*> paths) {
    for (const char* path : paths) {
        bool const shown = GuiTest::HoverShowsTooltip(ctx, path);
        if (!shown) ctx->LogError("no tooltip for '%s'", path);
        IM_CHECK_SILENT(shown);
    }
}

}

TEST_CASE("setup view controls explain themselves on hover", "[gui][tooltip]") {
    GuiTest::Harness harness;

    ImGuiTest* test = harness.NewTest("tip_setup");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ExpectTooltips(ctx, {"##game_profile", "##render_fps", "##render_preset"});
    };
    harness.Run(test);
}

TEST_CASE("top bar Export explains itself on hover when enabled", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_tip.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("tip_topbar");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        IM_CHECK(GuiTest::HoverShowsTooltip(ctx, ICON_EXPORT "  Export..."));
    };
    harness.Run(test);
}

TEST_CASE("status strip export tag and reveal button explain themselves", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_tip2.ifs", 0, 300);
    App::ExportState ex;
    ex.phase = App::ExportPhase::Done;
    ex.output_path = "C:/out/tip.webm";
    App::Global().SetExport(ex);

    ImGuiTest* test = harness.NewTest("tip_status_strip");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "status_strip");
        ExpectTooltips(ctx, {"export done", "Open folder"});
    };
    harness.Run(test);
}

TEST_CASE("status strip render error explains itself on hover", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    App::Status status = App::Global().GetStatus();
    status.scene_loaded = true;
    status.last_error = "afp_stream_create failed for a very long path that gets shortened";
    App::Global().SetStatus(status);

    ImGuiTest* test = harness.NewTest("tip_status_error");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "status_strip");
        const ImGuiWindow* strip = ctx->GetWindowByRef("");
        IM_CHECK(strip != nullptr);
        ctx->MouseMoveToPos(ImVec2(strip->Pos.x + 340.0F, strip->Pos.y + 12.0F));
        ctx->Yield(4);
        IM_CHECK(GuiTest::TooltipShown(ctx));
    };
    harness.Run(test);
}

TEST_CASE("timeline transport buttons all explain themselves", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_tip3.ifs", 40, 300);

    ImGuiTest* test = harness.NewTest("tip_transport");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/##timeline_dock");
        ExpectTooltips(ctx,
                       {ICON_JUMP_BACK, ICON_STEP_BACK, ICON_PAUSE, ICON_STEP_FWD, ICON_JUMP_FWD});
    };
    harness.Run(test);
}

TEST_CASE("timeline track and label combo explain themselves", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_tip4.ifs", 40, 300);
    App::Status status = App::Global().GetStatus();
    status.labels.push_back({.name = "intro", .frame = 0});
    App::Global().SetStatus(status);

    ImGuiTest* test = harness.NewTest("tip_track");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/##timeline_dock");
        ExpectTooltips(ctx, {"##tl_labels", "##tl_track"});
    };
    harness.Run(test);
}

TEST_CASE("inspector render tab controls all explain themselves", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_tip5.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("tip_inspector_render");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemClick("##inspector_tabs/Render");
        ctx->Yield(2);
        ctx->ItemCheck("##inspector_tabs/Render/Show MC names (F3)");
        ctx->Yield(2);
        ExpectTooltips(ctx, {
                                "##inspector_tabs/Render/Loop master animation",
                                "##inspector_tabs/Render/##root_loop/$$1/Force loop",
                                "##inspector_tabs/Render/##live_cont/$$2/ON",
                                "##inspector_tabs/Render/##live_trim",
                                "##inspector_tabs/Render/1.5x##scale_sdvx_old",
                                "##inspector_tabs/Render/##bg_color/$$5/blue",
                                "##inspector_tabs/Render/Filter (F7)",
                                "##inspector_tabs/Render/Show MC names (F3)",
                            });
    };
    harness.Run(test);
}

TEST_CASE("qpro controls explain themselves on hover", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "iidx33");

    ImGuiTest* test = harness.NewTest("tip_qpro");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        ctx->ItemClick("qpro");
        ctx->Yield(2);
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view");
        ExpectTooltips(ctx, {"Scan parts from bm2dx.dll", "Keep static base fills un-hue-shifted",
                             "Output fps"});
    };
    harness.Run(test);
}

TEST_CASE("export modal controls all explain themselves", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_tip6.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("tip_export");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        ctx->ItemClick(ICON_EXPORT "  Export...");
        ctx->SetRef("Export");
        GuiTest::ComboPick(ctx, "##exp_fmt", "WebM AV1 (video, opaque, NVENC)");
        ImGuiTestItemInfo const adv = ctx->ItemInfo("Advanced");
        if ((adv.StatusFlags & ImGuiItemStatusFlags_Opened) == 0) ctx->ItemClick("Advanced");
        ctx->ItemCheck("Limit frames##exp_limit");
        ctx->ItemCheck("Blend loop seam##exp_blend");
        ctx->Yield(3);
        ExpectTooltips(ctx, {"##exp_stem", "##exp_fmt", "fps##exp_fps", "##exp_q", "##exp_sz",
                             "Transparent bg", "HW accel##exp_hw", "keyframe interval##exp_keyint",
                             "Limit frames##exp_limit", "frames##exp_maxf",
                             "Continuous loop count##exp_loops", "Blend loop seam##exp_blend",
                             "Blend frames##exp_blendN", "Pick region##crop_pick"});
        ctx->ItemClick("Close");
    };
    harness.Run(test);
}

TEST_CASE("scene pane rows explain themselves on hover", "[gui][tooltip]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_tip7.ifs", 0, 300);
    {
        auto& cfg = App::Global().MutConfig("bg_tip7.ifs");
        cfg.filename = "bg_tip7.ifs";
        cfg.anim_names = {"bg_main"};
    }

    ImGuiTest* test = harness.NewTest("tip_scene_pane");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_center");
        ctx->ItemInputValue("##scene_filter", "");
        IM_CHECK(GuiTest::HoverShowsTooltip(ctx, "Add"));
        GuiTest::FocusChild(ctx, "scene_scroll");
        IM_CHECK(GuiTest::HoverShowsTooltip(ctx, "$$0/##layer"));
    };
    harness.Run(test);
}
