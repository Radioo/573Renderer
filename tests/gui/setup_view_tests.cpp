#include "gui_test_harness.h"

#include "arc_extract.h"
#include "gui_icons.h"
#include "customize_extract.h"
#include "imgui.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "state/app_state.h"
#include "state/commands.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <variant>

namespace {

template <typename T> const T* TakeAs(std::optional<App::Command>& slot) {
    slot = App::Global().TakeCommand();
    if (!slot.has_value()) return nullptr;
    return std::get_if<T>(&*slot);
}

template <typename IsRunningFn> bool WaitForJob(IsRunningFn is_running) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (is_running()) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
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

TEST_CASE("setup view hides the DDR extractors for other profiles", "[gui][setup]") {
    GuiTest::Harness harness;
    App::Global().SetGameProfileSlug("sdvx7");

    ImGuiTest* test = harness.NewTest("setup_no_extractors");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        IM_CHECK(ctx->ItemExists("Extract .arc files...") == false);
        IM_CHECK(ctx->ItemExists("Extract customize images...") == false);
    };
    harness.Run(test);
}

TEST_CASE("setup view offers the DDR extractors for ddrworld", "[gui][setup]") {
    GuiTest::Harness harness;
    App::Global().SetGameProfileSlug("ddrworld");

    ImGuiTest* test = harness.NewTest("setup_extractors_present");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        IM_CHECK(ctx->ItemExists("Extract .arc files..."));
        IM_CHECK(ctx->ItemExists("Extract customize images..."));
    };
    harness.Run(test);
}

TEST_CASE("setup view arc extractor does nothing when the picker is cancelled", "[gui][setup]") {
    GuiTest::Harness harness;
    App::Global().SetGameProfileSlug("ddrworld");
    GuiTest::SetBrowseResult("");

    ImGuiTest* test = harness.NewTest("setup_arc_cancel");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ctx->ItemClick("Extract .arc files...");
        ctx->ItemClick("Extract customize images...");
    };
    harness.Run(test);

    CHECK_FALSE(ArcExtract::IsRunning());
    CHECK_FALSE(CustomizeExtract::IsRunning());
}

TEST_CASE("setup view arc extractor runs over the picked folder", "[gui][setup]") {
    GuiTest::Harness harness;
    App::Global().SetGameProfileSlug("ddrworld");

    std::filesystem::path const dir =
        std::filesystem::temp_directory_path() / "r573_gui_arc_extract";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    REQUIRE_FALSE(ec);
    GuiTest::SetBrowseResult(dir.string());

    ImGuiTest* test = harness.NewTest("setup_arc_run");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ctx->ItemClick("Extract .arc files...");
    };
    harness.Run(test);

    REQUIRE(WaitForJob([] { return ArcExtract::IsRunning(); }));

    ArcExtract::Status const st = ArcExtract::GetStatus();
    CHECK_FALSE(st.running);
    CHECK(st.finished);
    CHECK(st.done_arcs == 0);
    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("setup view customize extractor runs over the picked folder", "[gui][setup]") {
    GuiTest::Harness harness;
    App::Global().SetGameProfileSlug("ddrworld");

    std::filesystem::path const dir =
        std::filesystem::temp_directory_path() / "r573_gui_customize_extract";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    REQUIRE_FALSE(ec);
    GuiTest::SetBrowseResult(dir.string());

    ImGuiTest* test = harness.NewTest("setup_customize_run");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ctx->ItemClick("Extract customize images...");
    };
    harness.Run(test);

    REQUIRE(WaitForJob([] { return CustomizeExtract::IsRunning(); }));

    CustomizeExtract::Status const st = CustomizeExtract::GetStatus();
    CHECK_FALSE(st.running);
    CHECK(st.finished);
    CHECK(st.written == 0);
    std::filesystem::remove_all(dir, ec);
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

TEST_CASE("setup view render size commits once, on edit completion", "[gui][setup]") {
    GuiTest::Harness harness;
    App::Global().SetRenderSize(1280, 720);

    ImGuiTest* test = harness.NewTest("setup_res_single_commit");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ctx->ItemClick("##render_w");
        ctx->KeyCharsReplace("1920");
        int mid_w = 0;
        int mid_h = 0;
        App::Global().GetRenderSize(mid_w, mid_h);
        IM_CHECK_EQ(mid_w, 1280);
        ctx->KeyPress(ImGuiKey_Enter);
    };
    harness.Run(test);

    int w = 0;
    int h = 0;
    App::Global().GetRenderSize(w, h);
    CHECK(w == 1920);
}

TEST_CASE("setup view fps commits once, on edit completion", "[gui][setup]") {
    GuiTest::Harness harness;
    App::Global().SetRenderFps(120);

    ImGuiTest* test = harness.NewTest("setup_fps_single_commit");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        ctx->ItemClick("##render_fps");
        ctx->KeyCharsReplace("144");
        IM_CHECK_EQ(App::Global().GetRenderFps(), 120);
        ctx->KeyPress(ImGuiKey_Enter);
    };
    harness.Run(test);

    CHECK(App::Global().GetRenderFps() == 144);
}

TEST_CASE("setup view Export tooltip is reachable while the button is disabled", "[gui][setup]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");

    ImGuiTest* test = harness.NewTest("shell_disabled_tooltip");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "topbar");
        ImGuiTestItemInfo const info = ctx->ItemInfo(ICON_EXPORT "  Export...");
        IM_CHECK((info.ItemFlags & ImGuiItemFlags_Disabled) != 0);
        ctx->MouseMove(ICON_EXPORT "  Export...");
        ctx->Yield(3);
        IM_CHECK(ctx->WindowInfo("//##Tooltip_00").Window != nullptr);
    };
    harness.Run(test);
}

TEST_CASE("setup view hides the 16:9 stretch for a non 4:3 render size", "[gui][setup]") {
    GuiTest::Harness harness;
    App::Global().SetRenderSize(1280, 720);
    App::Global().SetStretchWide(false);

    ImGuiTest* test = harness.NewTest("setup_stretch_hidden");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        IM_CHECK(ctx->ItemExists("Stretch to 16:9") == false);
    };
    harness.Run(test);
}

TEST_CASE("setup view offers the 16:9 stretch and its filter for 4:3", "[gui][setup]") {
    GuiTest::Harness harness;
    App::Global().SetRenderSize(640, 480);
    App::Global().SetStretchWide(false);

    ImGuiTest* test = harness.NewTest("setup_stretch_shown");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##setup");
        GuiTest::FocusChild(ctx, "setup_card");
        IM_CHECK(ctx->ItemExists("Stretch to 16:9"));
        IM_CHECK(ctx->ItemExists("Scaling##stretch_filter") == false);
        ctx->ItemClick("Stretch to 16:9");
        IM_CHECK(App::Global().GetStretchWide());
        IM_CHECK(ctx->ItemExists("Scaling##stretch_filter"));
    };
    harness.Run(test);
    App::Global().SetStretchWide(false);
}
