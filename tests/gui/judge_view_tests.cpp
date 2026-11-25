#include "gui_test_harness.h"

#include "gui/judge/judge_view.h"
#include "gui_icons.h"
#include "imgui.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "judge/afp_pose.h"
#include "judge/clip_catalog.h"
#include "judge/verdict_store.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

#ifdef Yield
#undef Yield
#endif

namespace {

struct Recorded {
    std::optional<Judge::Verdict> verdict;
    std::optional<std::size_t> select;
    bool note_edited = false;
    bool toggle_pause = false;
    bool step = false;
    bool restart = false;
    bool toggle_loop = false;
    bool toggle_next = false;
    std::optional<std::size_t> scale_index;
    std::optional<AfpPose::MatrixLerp> lerp;
};

Recorded* g_recorded = nullptr;

struct Fixture {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / "r573_judge_view_tests.json";
    std::optional<Judge::VerdictStore> store;
    Judge::Catalog catalog{{{.package = "a.bin", .path = "a.bin", .clips = {"one", "two"}},
                            {.package = "b.bin", .path = "b.bin", .clips = {"three"}}}};
    JudgeView::Model model;
    std::string note;
    Recorded recorded;

    Fixture() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        std::string err;
        store = Judge::VerdictStore::Open(path.string(), err);
        REQUIRE(store);
        if (!store) return;
        REQUIRE(store->Set(catalog.Key(0), {.verdict = Judge::Verdict::Smooth, .note = {}}));
        model.catalog = &catalog;
        model.store = &*store;
        model.scan.done = true;
        g_recorded = &recorded;
        GuiTest::SetFrameBuilder([this] {
            const JudgeView::Actions a = JudgeView::Build(model, note);
            if (a.verdict) recorded.verdict = a.verdict;
            if (a.select) recorded.select = a.select;
            if (a.lerp) recorded.lerp = a.lerp;
            recorded.note_edited = recorded.note_edited || a.note_edited;
            recorded.toggle_pause = recorded.toggle_pause || a.toggle_pause;
            recorded.step = recorded.step || a.step;
            recorded.restart = recorded.restart || a.restart;
            recorded.toggle_loop = recorded.toggle_loop || a.toggle_loop;
            recorded.toggle_next = recorded.toggle_next || a.toggle_next_after_verdict;
            if (a.scale_index) recorded.scale_index = a.scale_index;
        });
    }
    ~Fixture() {
        g_recorded = nullptr;
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;
    Fixture(Fixture&&) = delete;
    Fixture& operator=(Fixture&&) = delete;
};

}

TEST_CASE("The judge view shows how many clips are judged and the verdict of each row",
          "[gui][judge]") {
    GuiTest::Harness harness;
    const Fixture fixture;
    ImGuiTest* test = harness.NewTest("judge_progress");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->Yield(2);
        const std::string text = GuiTest::CaptureFrameText(ctx);
        IM_CHECK(text.find("1 of 3 judged") != std::string::npos);
        IM_CHECK(text.find("Smooth") != std::string::npos);
        IM_CHECK(text.find("a.bin / one") != std::string::npos);
    };
    harness.Run(test);
}

TEST_CASE("Verdict buttons, keys, rows and the note reach the judge", "[gui][judge]") {
    GuiTest::Harness harness;
    Fixture fixture;
    ImGuiTest* test = harness.NewTest("judge_actions");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        const Recorded& recorded = *g_recorded;
        ctx->SetRef("##judge");
        ctx->ItemClick("**/Wrong (2)");
        IM_CHECK(recorded.verdict == Judge::Verdict::Wrong);
        ctx->KeyPress(ImGuiKey_3);
        IM_CHECK(recorded.verdict == Judge::Verdict::Unsure);
        ctx->ItemClick("**/b.bin  three");
        IM_CHECK(recorded.select == 2U);
        ctx->ItemClick("**/Linear");
        IM_CHECK(recorded.lerp == AfpPose::MatrixLerp::Linear);
        ctx->KeyPress(ImGuiKey_Space);
        IM_CHECK(recorded.toggle_pause);
        ctx->ItemClick("**/##note");
        ctx->KeyCharsAppend("ring snaps");
        IM_CHECK(recorded.note_edited);
        ctx->KeyPress(ImGuiKey_2);
        IM_CHECK(recorded.verdict == Judge::Verdict::Unsure);
    };
    harness.Run(test);
    CHECK(fixture.note == "ring snaps");
}

TEST_CASE("Transport keys and buttons and clip navigation reach the judge", "[gui][judge]") {
    GuiTest::Harness harness;
    Fixture fixture;
    fixture.model.scale_index = 1;
    ImGuiTest* test = harness.NewTest("judge_transport");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        Recorded& recorded = *g_recorded;
        ctx->SetRef("##judge");
        ctx->KeyPress(ImGuiKey_DownArrow);
        IM_CHECK(recorded.select == 1U);
        recorded.select.reset();
        ctx->KeyPress(ImGuiKey_N);
        IM_CHECK(recorded.select == 1U);
        ctx->KeyPress(ImGuiKey_RightArrow);
        IM_CHECK(recorded.step);
        recorded.step = false;
        ctx->ItemClick("**/" ICON_STEP_FWD "  Step");
        IM_CHECK(recorded.step);
        ctx->KeyPress(ImGuiKey_R);
        IM_CHECK(recorded.restart);
        recorded.restart = false;
        ctx->ItemClick("**/" ICON_JUMP_BACK "  Restart");
        IM_CHECK(recorded.restart);
        ctx->KeyPress(ImGuiKey_L);
        IM_CHECK(recorded.toggle_loop);
        ctx->KeyPress(ImGuiKey_LeftBracket);
        IM_CHECK(recorded.scale_index == 2U);
        ctx->KeyPress(ImGuiKey_RightBracket);
        IM_CHECK(recorded.scale_index == 0U);
        ctx->ItemClick("**/0.25x");
        IM_CHECK(recorded.scale_index == 2U);
        ctx->ItemClick("**/Next unjudged clip after a verdict");
        IM_CHECK(recorded.toggle_next);
    };
    harness.Run(test);
}
