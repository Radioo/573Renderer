#include "gui_test_harness.h"

#include "gui_icons.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "state/app_state.h"
#include "state/telemetry.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("shell shows the setup view until boot reports ready", "[gui][shell]") {
    GuiTest::Harness harness;

    ImGuiTest* test = harness.NewTest("shell_setup_visible");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        IM_CHECK(ctx->WindowInfo("##setup").Window != nullptr);
        IM_CHECK(ctx->WindowInfo("##main", ImGuiTestOpFlags_NoError).Window == nullptr);
    };
    harness.Run(test);
}

TEST_CASE("shell swaps to the ready view once boot completes", "[gui][shell]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");

    ImGuiTest* test = harness.NewTest("shell_ready_visible");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        IM_CHECK(ctx->WindowInfo("##main").Window != nullptr);
        IM_CHECK(ctx->WindowInfo("##setup", ImGuiTestOpFlags_NoError).Window == nullptr);
    };
    harness.Run(test);
}

TEST_CASE("top bar Export is disabled until a scene is loaded", "[gui][shell]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");

    ImGuiTest* test = harness.NewTest("shell_export_disabled");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        ImGuiTestItemInfo const info = ctx->ItemInfo(ICON_EXPORT "  Export...");
        IM_CHECK_NE(info.ID, 0U);
        IM_CHECK((info.ItemFlags & ImGuiItemFlags_Disabled) != 0);
    };
    harness.Run(test);

    CHECK_FALSE(App::Global().TakeCommand().has_value());
}

TEST_CASE("top bar Export opens the export modal once a scene is loaded", "[gui][shell]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_0001.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("shell_export_opens");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        ctx->ItemClick("\xEE\xA2\x98"
                       "  Export...");
        ctx->Yield(2);
        IM_CHECK(ctx->WindowInfo("//Export").Window != nullptr);
    };
    harness.Run(test);
}

TEST_CASE("top bar hides the view switch when only one main panel is active", "[gui][shell]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");

    ImGuiTest* test = harness.NewTest("shell_no_view_switch");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        IM_CHECK(ctx->ItemExists("qpro") == false);
        IM_CHECK(ctx->ItemExists("Renderer") == false);
    };
    harness.Run(test);
}

TEST_CASE("top bar view switch appears for iidx33 and swaps the main panel", "[gui][shell]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "iidx33");

    ImGuiTest* test = harness.NewTest("shell_view_switch");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        IM_CHECK(ctx->ItemExists("Renderer"));
        ctx->ItemClick("qpro");
        ctx->Yield(2);
        ctx->SetRef("##main");
        IM_CHECK(ctx->WindowInfo("main_view/pane_left", ImGuiTestOpFlags_NoError).Window ==
                 nullptr);
        GuiTest::FocusChild(ctx, "topbar");
        ctx->ItemClick("Renderer");
        ctx->Yield(2);
        ctx->SetRef("##main");
        IM_CHECK(ctx->WindowInfo("main_view/pane_left").Window != nullptr);
    };
    harness.Run(test);
}

TEST_CASE("inspector exposes the modern backend tab set", "[gui][shell]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");

    ImGuiTest* test = harness.NewTest("shell_tabs_modern");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Properties"));
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Render"));
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Live"));
        IM_CHECK(ctx->ItemExists("##inspector_tabs/3D scene") == false);
    };
    harness.Run(test);
}

TEST_CASE("inspector exposes the ddr backend tab set", "[gui][shell]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_ddr", "ddrworld");

    ImGuiTest* test = harness.NewTest("shell_tabs_ddr");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Render"));
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Live"));
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Properties") == false);
    };
    harness.Run(test);
}

TEST_CASE("inspector hides host tabs while no 3D scene or 2D package is live", "[gui][shell]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("scene3d", "iidx18");

    ImGuiTest* test = harness.NewTest("shell_tabs_scene3d_inactive");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        IM_CHECK(ctx->ItemExists("##inspector_tabs/3D scene") == false);
        IM_CHECK(ctx->ItemExists("##inspector_tabs/2D package") == false);
    };
    harness.Run(test);
}

TEST_CASE("inspector tabs switch the visible body", "[gui][shell]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");

    ImGuiTest* test = harness.NewTest("shell_tab_switch");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemClick("##inspector_tabs/Render");
        ctx->Yield(2);
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Render/Loop master animation"));
        ctx->ItemClick("##inspector_tabs/Properties");
        ctx->Yield(2);
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Render/Loop master animation") == false);
    };
    harness.Run(test);
}

TEST_CASE("splitter drag moves the boundary between the left and centre panes", "[gui][shell]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");

    ImGuiTest* test = harness.NewTest("shell_splitter_drag");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        ImGuiTestItemInfo const before = ctx->WindowInfo("main_view/pane_left");
        IM_CHECK(before.Window != nullptr);
        float const w_before = before.Window->Size.x;

        GuiTest::FocusChild(ctx, "main_view");
        ctx->MouseMove("##split_l");
        ctx->MouseDown(0);
        ctx->MouseMoveToPos(ImVec2(ImGui::GetIO().MousePos.x + 60.0F, ImGui::GetIO().MousePos.y));
        ctx->MouseUp(0);
        ctx->Yield(2);

        ctx->SetRef("##main");
        ImGuiTestItemInfo const after = ctx->WindowInfo("main_view/pane_left");
        IM_CHECK(after.Window != nullptr);
        IM_CHECK_GT(after.Window->Size.x, w_before);
    };
    harness.Run(test);
}

TEST_CASE("status strip export tag reopens the export modal", "[gui][shell]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_0001.ifs", 0, 300);
    App::ExportState ex;
    ex.phase = App::ExportPhase::Done;
    ex.output_path = "C:/out/bg_0001.webm";
    App::Global().SetExport(ex);

    ImGuiTest* test = harness.NewTest("shell_status_tag_reopens");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "status_strip");
        ctx->ItemClick("export done");
        ctx->Yield(2);
        IM_CHECK(ctx->WindowInfo("//Export").Window != nullptr);
    };
    harness.Run(test);
}

TEST_CASE("status strip hides the reveal button unless an export finished", "[gui][shell]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_0001.ifs", 0, 300);
    App::ExportState ex;
    ex.phase = App::ExportPhase::Capturing;
    ex.frames_captured = 12;
    App::Global().SetExport(ex);

    ImGuiTest* test = harness.NewTest("shell_status_tag_capturing");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "status_strip");
        IM_CHECK(ctx->ItemExists("Open folder") == false);
        IM_CHECK(ctx->ItemExists("capturing 12"));
    };
    harness.Run(test);

    CHECK(GuiTest::TakeRevealedPath().empty());
}
