#include "gui_test_harness.h"

#include "imgui.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "state/app_state.h"
#include "state/boot_lifecycle.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("3D scene panel explains how to load a scene while none is live", "[gui][hosts]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_ddr", "ddrworld");

    ImGuiTest* test = harness.NewTest("hosts_scene3d_idle");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        IM_CHECK(ctx->ItemExists("##inspector_tabs/3D scene") == false);
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Render/Pause##s3d") == false);
    };
    harness.Run(test);
}

TEST_CASE("2D package panel stays hidden while no package is live", "[gui][hosts]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("scene3d", "iidx17");

    ImGuiTest* test = harness.NewTest("hosts_gc2d_idle");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        IM_CHECK(ctx->ItemExists("##inspector_tabs/2D package") == false);
        IM_CHECK(ctx->ItemExists("##gc2danim") == false);
    };
    harness.Run(test);
}

TEST_CASE("loading overlay covers the setup view while a load runs", "[gui][hosts]") {
    GuiTest::Harness harness;
    App::Global().BeginLoad("bg_0001.ifs");
    App::Global().UpdateLoadStage("decoding textures", 0.5F);

    ImGuiTest* test = harness.NewTest("hosts_overlay_setup");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        IM_CHECK(ctx->WindowInfo("##loading_overlay").Window != nullptr);
    };
    harness.Run(test);

    App::Global().SetLoadProgress({});
}

TEST_CASE("loading overlay covers the ready view while a load runs", "[gui][hosts]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    App::Global().BeginLoad("bg_0002.ifs");
    App::Global().SetTexturesExpected(10);
    App::Global().BumpTexturesLoaded();

    ImGuiTest* test = harness.NewTest("hosts_overlay_ready");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        IM_CHECK(ctx->WindowInfo("##loading_overlay").Window != nullptr);
        IM_CHECK(ctx->WindowInfo("##main").Window != nullptr);
    };
    harness.Run(test);

    App::Global().SetLoadProgress({});
}

TEST_CASE("loading overlay is absent while nothing is loading", "[gui][hosts]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");

    ImGuiTest* test = harness.NewTest("hosts_overlay_absent");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        IM_CHECK(ctx->WindowInfo("##loading_overlay", ImGuiTestOpFlags_NoError).Window == nullptr);
    };
    harness.Run(test);
}

TEST_CASE("setup view surfaces the last boot failure", "[gui][hosts]") {
    GuiTest::Harness harness;
    App::Global().SetBootState(App::BootState::Failed);
    App::Global().SetBootError("afp-core.dll not found");

    ImGuiTest* test = harness.NewTest("hosts_boot_error");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        IM_CHECK(ctx->WindowInfo("setup_card/setup_err").Window != nullptr);
    };
    harness.Run(test);
}

TEST_CASE("setup view disables Load while a boot is in flight", "[gui][hosts]") {
    GuiTest::Harness harness;
    App::Global().SetGameDir("C:/games/iidx");
    App::Global().SetBootState(App::BootState::Booting);

    ImGuiTest* test = harness.NewTest("hosts_booting");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ImGuiTestItemInfo const info = ctx->ItemInfo("Load");
        IM_CHECK_NE(info.ID, 0U);
        IM_CHECK((info.ItemFlags & ImGuiItemFlags_Disabled) != 0);
    };
    harness.Run(test);

    CHECK_FALSE(App::Global().TakeCommand().has_value());
}
