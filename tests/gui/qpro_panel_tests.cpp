#include "gui_test_harness.h"

#include "backend/afp_commands.h"
#include "imgui.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "state/app_state.h"
#include "state/commands.h"

#include <any>
#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <variant>

namespace {

void OpenQproView(ImGuiTestContext* ctx) {
    ctx->SetRef("##main");
    GuiTest::FocusChild(ctx, "topbar");
    ctx->ItemClick("qpro");
    ctx->Yield(2);
    ctx->SetRef("##main");
    GuiTest::FocusChild(ctx, "main_view");
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

TEST_CASE("qpro tab warns unless the render size matches the avatar", "[gui][qpro]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "iidx33");
    App::Global().SetRenderSize(1920, 1080);

    ImGuiTest* test = harness.NewTest("qpro_size_warning");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenQproView(ctx);
        IM_CHECK(ctx->ItemExists("Keep static base fills un-hue-shifted"));
        IM_CHECK(ctx->ItemExists("Head"));
    };
    harness.Run(test);
}

TEST_CASE("qpro category checkboxes gate the extract button", "[gui][qpro]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "iidx33");

    ImGuiTest* test = harness.NewTest("qpro_categories_gate");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenQproView(ctx);
        for (const char* cat : {"Head", "Hand", "Hair", "Face", "Body", "Back"})
            ctx->ItemUncheck(cat);
        ctx->Yield(2);
        ImGuiTestItemInfo const info = ctx->ItemInfo("Choose output folder + extract...");
        IM_CHECK_NE(info.ID, 0U);
        IM_CHECK((info.ItemFlags & ImGuiItemFlags_Disabled) != 0);
        ctx->ItemCheck("Head");
        ctx->Yield(2);
        ImGuiTestItemInfo const after = ctx->ItemInfo("Choose output folder + extract...");
        IM_CHECK((after.ItemFlags & ImGuiItemFlags_Disabled) == 0);
    };
    harness.Run(test);

    CHECK_FALSE(App::Global().TakeCommand().has_value());
}

TEST_CASE("qpro extract posts the selected options", "[gui][qpro]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "iidx33");
    GuiTest::SetBrowseResult("D:/qpro_out");

    ImGuiTest* test = harness.NewTest("qpro_extract");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenQproView(ctx);
        for (const char* cat : {"Head", "Hand", "Hair", "Face", "Body", "Back"})
            ctx->ItemCheck(cat);
        ctx->ItemUncheck("Back");
        ctx->ItemUncheck("Keep static base fills un-hue-shifted");
        ctx->ItemInputValue("Output fps", 30);
        ctx->ItemClick("Choose output folder + extract...");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* extract = TakeAfpCommand<AfpCmd::QproStartExtract>(slot);
    REQUIRE(extract != nullptr);
    CHECK(extract->out_dir == "D:/qpro_out");
    CHECK(extract->fps == 30);
    CHECK_FALSE(extract->hue_scope);
    CHECK(extract->parts.head);
    CHECK_FALSE(extract->parts.back);
}

TEST_CASE("qpro extract is abandoned when the folder picker is cancelled", "[gui][qpro]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "iidx33");
    GuiTest::SetBrowseResult("");

    ImGuiTest* test = harness.NewTest("qpro_extract_cancel");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenQproView(ctx);
        ctx->ItemCheck("Head");
        ctx->ItemClick("Choose output folder + extract...");
    };
    harness.Run(test);

    CHECK_FALSE(App::Global().TakeCommand().has_value());
}

TEST_CASE("qpro output fps clamps to the accepted range", "[gui][qpro]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "iidx33");
    GuiTest::SetBrowseResult("D:/qpro_clamped");

    ImGuiTest* test = harness.NewTest("qpro_fps_clamp");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenQproView(ctx);
        ctx->ItemCheck("Head");
        ctx->ItemInputValue("Output fps", 9000);
        ctx->ItemClick("Choose output folder + extract...");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* extract = TakeAfpCommand<AfpCmd::QproStartExtract>(slot);
    REQUIRE(extract != nullptr);
    CHECK(extract->fps == 240);
}

TEST_CASE("qpro scan button posts the scan command", "[gui][qpro]") {
    GuiTest::Harness harness;
    GuiTest::EnterReadyView("afp_modern", "iidx33");

    ImGuiTest* test = harness.NewTest("qpro_scan");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenQproView(ctx);
        ctx->ItemClick("Scan parts from bm2dx.dll");
    };
    harness.Run(test);

    std::optional<App::Command> slot;
    const auto* scan = TakeAfpCommand<AfpCmd::QproStartScan>(slot);
    CHECK(scan != nullptr);
}
