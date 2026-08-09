#include "gui_test_harness.h"

#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "state/app_state.h"
#include "state/commands.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace {

void SeedCatalog() {
    std::vector<App::State::IfsEntry> entries;
    entries.push_back({.name = "bg_0001.ifs", .full_path = "bg_0001.ifs", .from_arc = false});
    entries.push_back({.name = "bg_0002.ifs", .full_path = "bg_0002.ifs", .from_arc = false});
    entries.push_back({.name = "packed.arc", .full_path = "packed.arc", .from_arc = true});
    App::Global().SetAvailableIfs(std::move(entries));
}

void ClearFilter(ImGuiTestContext* ctx) {
    ctx->SetRef("##main");
    GuiTest::FocusChild(ctx, "main_view/pane_left");
    ctx->ItemInputValue("##ifsfilter", "");
}

const App::Cmd::LoadContent* TakeLoad(std::optional<App::Command>& slot) {
    slot = App::Global().TakeCommand();
    if (!slot.has_value()) return nullptr;
    return std::get_if<App::Cmd::LoadContent>(&*slot);
}

}

TEST_CASE("browse pane reports an empty catalog", "[gui][browse]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");

    ImGuiTest* test = harness.NewTest("browse_empty");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_left");
        IM_CHECK(ctx->ItemExists("##ifsfilter") == false);
    };
    harness.Run(test);
}

TEST_CASE("browse pane hides the tree while a scan is running", "[gui][browse]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    SeedCatalog();
    App::Global().SetIfsScanning(true);
    App::Global().SetIfsScanStatus("walking data/graphic");

    ImGuiTest* test = harness.NewTest("browse_scanning");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_left");
        IM_CHECK(ctx->ItemExists("##ifsfilter") == false);
    };
    harness.Run(test);

    App::Global().SetIfsScanning(false);
}

TEST_CASE("browse pane selection posts LoadContent for the picked IFS", "[gui][browse]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    SeedCatalog();

    ImGuiTest* test = harness.NewTest("browse_select");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ClearFilter(ctx);
        GuiTest::FocusChild(ctx, "ifs_scroll");
        ctx->ItemClick("bg_0002.ifs/bg_0002.ifs");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* load = TakeLoad(slot);
    REQUIRE(load != nullptr);
    CHECK(load->path == "bg_0002.ifs");
    CHECK_FALSE(load->from_arc);
}

TEST_CASE("browse pane carries the from_arc flag through LoadContent", "[gui][browse]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    SeedCatalog();

    ImGuiTest* test = harness.NewTest("browse_select_arc");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ClearFilter(ctx);
        GuiTest::FocusChild(ctx, "ifs_scroll");
        ctx->ItemClick("packed.arc/packed.arc");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* load = TakeLoad(slot);
    REQUIRE(load != nullptr);
    CHECK(load->path == "packed.arc");
    CHECK(load->from_arc);
}

TEST_CASE("browse pane does not reload the already-active IFS", "[gui][browse]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    SeedCatalog();
    GuiTest::LoadScene("bg_0001.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("browse_select_active");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ClearFilter(ctx);
        GuiTest::FocusChild(ctx, "ifs_scroll");
        ctx->ItemClick("bg_0001.ifs/bg_0001.ifs");
    };
    harness.Run(test);

    CHECK_FALSE(App::Global().TakeCommand().has_value());
}

TEST_CASE("browse pane filter hides non-matching entries", "[gui][browse]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    SeedCatalog();

    ImGuiTest* test = harness.NewTest("browse_filter");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_left");
        ctx->ItemInputValue("##ifsfilter", "0002");
        ctx->Yield(2);
        GuiTest::FocusChild(ctx, "ifs_scroll");
        IM_CHECK(ctx->ItemExists("bg_0002.ifs/bg_0002.ifs"));
        IM_CHECK(ctx->ItemExists("bg_0001.ifs/bg_0001.ifs") == false);
        IM_CHECK(ctx->ItemExists("packed.arc/packed.arc") == false);
        ClearFilter(ctx);
    };
    harness.Run(test);
}

TEST_CASE("browse pane groups nested paths under expandable directories", "[gui][browse]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    std::vector<App::State::IfsEntry> entries;
    entries.push_back({.name = "graphic/deep.ifs", .full_path = "deep_full", .from_arc = false});
    App::Global().SetAvailableIfs(std::move(entries));

    ImGuiTest* test = harness.NewTest("browse_tree_dirs");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ClearFilter(ctx);
        GuiTest::FocusChild(ctx, "ifs_scroll");
        IM_CHECK(ctx->ItemExists("graphic/##dir"));
        ctx->ItemClick("graphic/##dir/deep_full/deep.ifs");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* load = TakeLoad(slot);
    REQUIRE(load != nullptr);
    CHECK(load->path == "deep_full");
}

TEST_CASE("browse pane directory node collapses on click", "[gui][browse]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    std::vector<App::State::IfsEntry> entries;
    entries.push_back({.name = "graphic/deep.ifs", .full_path = "deep_full", .from_arc = false});
    App::Global().SetAvailableIfs(std::move(entries));

    ImGuiTest* test = harness.NewTest("browse_tree_collapse");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ClearFilter(ctx);
        GuiTest::FocusChild(ctx, "ifs_scroll");
        IM_CHECK(ctx->ItemExists("graphic/##dir/deep_full/deep.ifs"));
        ctx->ItemClose("graphic/##dir");
        ctx->Yield(2);
        IM_CHECK(ctx->ItemExists("graphic/##dir/deep_full/deep.ifs") == false);
        ctx->ItemOpen("graphic/##dir");
        ctx->Yield(2);
        IM_CHECK(ctx->ItemExists("graphic/##dir/deep_full/deep.ifs"));
    };
    harness.Run(test);

    CHECK_FALSE(App::Global().TakeCommand().has_value());
}
