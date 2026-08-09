#include "gui_test_harness.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "state/ifs_catalog.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

void SeedManyIfs(int count) {
    std::vector<App::State::IfsEntry> entries;
    entries.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; i++) {
        std::string const name = "bg_" + std::to_string(1000 + i) + ".ifs";
        entries.push_back({.name = name, .full_path = name, .from_arc = false});
    }
    App::Global().SetAvailableIfs(std::move(entries));
}

}

TEST_CASE("keyboard activation runs the same path as a click", "[gui][nav]") {
    GuiTest::Harness harness;
    App::Global().SetGameDir("C:/games/nav");

    ImGuiTest* test = harness.NewTest("nav_activate_load");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ctx->SetInputMode(ImGuiInputSource_Keyboard);
        ctx->ItemClick("Load");
        ctx->SetInputMode(ImGuiInputSource_Mouse);
    };
    harness.Run(test);

    std::optional<App::Command> const cmd = App::Global().TakeCommand();
    REQUIRE(cmd.has_value());
    const auto* boot = std::get_if<App::Cmd::BootGame>(&*cmd);
    REQUIRE(boot != nullptr);
    CHECK(boot->game_dir == "C:/games/nav");
}

TEST_CASE("keyboard nav moves focus between setup controls", "[gui][nav]") {
    GuiTest::Harness harness;

    ImGuiTest* test = harness.NewTest("nav_focus_move");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ctx->SetInputMode(ImGuiInputSource_Keyboard);
        ctx->NavMoveTo("Browse...");
        IM_CHECK_EQ(ctx->UiContext->NavId, ctx->GetID("Browse..."));
        ctx->NavMoveTo("##game_profile");
        IM_CHECK_EQ(ctx->UiContext->NavId, ctx->GetID("##game_profile"));
        ctx->SetInputMode(ImGuiInputSource_Mouse);
    };
    harness.Run(test);
}

TEST_CASE("keyboard nav activates a transport button", "[gui][nav]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_nav.ifs", 10, 300);

    ImGuiTest* test = harness.NewTest("nav_activate_transport");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/##timeline_dock");
        ctx->SetInputMode(ImGuiInputSource_Keyboard);
        ctx->NavMoveTo("\xEE\x9D\xAC");
        ctx->NavActivate();
        ctx->SetInputMode(ImGuiInputSource_Mouse);
    };
    harness.Run(test);

    CHECK(App::Global().TakeCommand().has_value());
}

TEST_CASE("browse tree scrolls when the catalog overflows the pane", "[gui][scroll]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    SeedManyIfs(120);

    ImGuiTest* test = harness.NewTest("scroll_browse_tree");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_left/ifs_scroll");
        const ImGuiWindow* w = ctx->GetWindowByRef("");
        IM_CHECK(w != nullptr);
        IM_CHECK_GT(w->ScrollMax.y, 0.0F);

        float const before = w->Scroll.y;
        ctx->MouseMove("bg_1000.ifs/bg_1000.ifs");
        ctx->MouseWheelY(-5.0F);
        ctx->Yield(3);
        IM_CHECK_GT(w->Scroll.y, before);

        ctx->ScrollToTop("");
        ctx->Yield(2);
        IM_CHECK_EQ(w->Scroll.y, 0.0F);
    };
    harness.Run(test);
}

TEST_CASE("export modal stays inside a short window and keeps its footer", "[gui][scroll]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_scroll.ifs", 0, 300);
    GuiTest::SetDisplaySize(900.0F, 520.0F);

    ImGuiTest* test = harness.NewTest("scroll_export_modal");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        ctx->ItemClick("\xEE\xA2\x98"
                       "  Export...");
        ctx->SetRef("Export");
        ImGuiTestItemInfo const adv = ctx->ItemInfo("Advanced");
        if ((adv.StatusFlags & ImGuiItemStatusFlags_Opened) == 0) ctx->ItemClick("Advanced");
        ctx->Yield(4);

        const ImGuiWindow* w = ctx->GetWindowByRef("");
        IM_CHECK(w != nullptr);
        IM_CHECK_LE(w->Size.y, ImGui::GetMainViewport()->WorkSize.y);
        IM_CHECK_GT(w->ScrollMax.y, 0.0F);

        ctx->ScrollToBottom("");
        ctx->Yield(2);
        ctx->ItemClick("Start export");
    };
    harness.Run(test);

    std::optional<App::Command> const cmd = App::Global().TakeCommand();
    REQUIRE(cmd.has_value());
    CHECK(std::holds_alternative<App::Cmd::StartExport>(*cmd));
}

TEST_CASE("scene tree scrolls when a package has many layers", "[gui][scroll]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_manylayers.ifs", 0, 300);
    {
        auto& cfg = App::Global().MutConfig("bg_manylayers.ifs");
        cfg.filename = "bg_manylayers.ifs";
        for (int i = 0; i < 80; i++)
            cfg.anim_names.push_back("layer_" + std::to_string(i));
    }

    ImGuiTest* test = harness.NewTest("scroll_scene_tree");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_center/scene_scroll");
        const ImGuiWindow* w = ctx->GetWindowByRef("");
        IM_CHECK(w != nullptr);
        IM_CHECK_GT(w->ScrollMax.y, 0.0F);
        ctx->ScrollToBottom("");
        ctx->Yield(2);
        IM_CHECK_GT(w->Scroll.y, 0.0F);
    };
    harness.Run(test);
}
