#include "gui_test_harness.h"

#include "imgui.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "state/app_state.h"
#include "state/commands.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <variant>

namespace {

template <typename T> const T* TakeAs(std::optional<App::Command>& slot) {
    slot = App::Global().TakeCommand();
    if (!slot.has_value()) return nullptr;
    return std::get_if<T>(&*slot);
}

}

TEST_CASE("setup view Load posts BootGame with the typed directory", "[gui][setup]") {
    GuiTest::Harness harness;

    ImGuiTest* test = harness.NewTest("setup_load");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ctx->ItemInputValue("##gamedir", "C:/games/ddr");
        ctx->ItemClick("Load");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* boot = TakeAs<App::Cmd::BootGame>(slot);
    REQUIRE(boot != nullptr);
    CHECK(boot->game_dir == "C:/games/ddr");
    CHECK(boot->render_width == 1280);
    CHECK(boot->render_height == 720);
}

TEST_CASE("setup view Load is disabled while no directory is set", "[gui][setup]") {
    GuiTest::Harness harness;

    ImGuiTest* test = harness.NewTest("setup_load_disabled");
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

TEST_CASE("setup view Browse adopts the picked folder", "[gui][setup]") {
    GuiTest::Harness harness;
    GuiTest::SetBrowseResult("D:/konami/sdvx");

    ImGuiTest* test = harness.NewTest("setup_browse");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ctx->ItemClick("Browse...");
    };
    harness.Run(test);

    CHECK(App::Global().GameDir() == "D:/konami/sdvx");
}

TEST_CASE("setup view Browse keeps the current folder when cancelled", "[gui][setup]") {
    GuiTest::Harness harness;
    App::Global().SetGameDir("E:/keep/me");
    GuiTest::SetBrowseResult("");

    ImGuiTest* test = harness.NewTest("setup_browse_cancel");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ctx->ItemClick("Browse...");
    };
    harness.Run(test);

    CHECK(App::Global().GameDir() == "E:/keep/me");
}

TEST_CASE("setup view fps quick button sets the render fps", "[gui][setup]") {
    GuiTest::Harness harness;

    ImGuiTest* test = harness.NewTest("setup_fps_quick");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ctx->ItemClick("60##fps_60");
    };
    harness.Run(test);

    CHECK(App::Global().GetRenderFps() == 60);
}

TEST_CASE("setup view profile combo sets the game profile slug", "[gui][setup]") {
    GuiTest::Harness harness;

    ImGuiTest* test = harness.NewTest("setup_profile_combo");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ctx->ComboClick("##game_profile/DDR World (MDX)");
    };
    harness.Run(test);

    CHECK(App::Global().GetGameProfileSlug() == "ddrworld");
}

TEST_CASE("setup view resolution preset sets the render size", "[gui][setup]") {
    GuiTest::Harness harness;
    App::Global().SetRenderSize(640, 480);

    ImGuiTest* test = harness.NewTest("setup_res_preset");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ctx->ComboClick("##render_preset/1920x1080 (IIDX)");
    };
    harness.Run(test);

    int w = 0;
    int h = 0;
    App::Global().GetRenderSize(w, h);
    CHECK(w == 1920);
    CHECK(h == 1080);
}

TEST_CASE("setup view custom resolution inputs clamp and apply", "[gui][setup]") {
    GuiTest::Harness harness;

    ImGuiTest* test = harness.NewTest("setup_res_custom");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ctx->ItemInputValue("##render_w", 900);
        ctx->ItemInputValue("##render_h", 20);
    };
    harness.Run(test);

    int w = 0;
    int h = 0;
    App::Global().GetRenderSize(w, h);
    CHECK(w == 900);
    CHECK(h == 64);
}
