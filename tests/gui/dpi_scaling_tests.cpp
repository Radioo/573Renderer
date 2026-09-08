#include "gui_test_harness.h"

#include "gui_dpi.h"
#include "gui_layout_constants.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace {

struct ShellMetrics {
    float top_bar_h = 0.0F;
    float status_h = 0.0F;
    float timeline_h = 0.0F;
    float text_h = 0.0F;
};

ShellMetrics& Captured() {
    static ShellMetrics metrics;
    return metrics;
}

void CheckChildFits(ImGuiTestContext* ctx, const char* path) {
    const ImGuiWindow* window = ctx->WindowInfo(path).Window;
    IM_CHECK_SILENT(window != nullptr);
    if (window->ContentSize.y > window->InnerRect.GetHeight()) {
        ctx->LogError("%s stacks %.1f px of content into %.1f px of room", path,
                      window->ContentSize.y, window->InnerRect.GetHeight());
    }
    IM_CHECK_LE(window->ContentSize.y, window->InnerRect.GetHeight());
}

ShellMetrics MeasureShell(unsigned dpi) {
    Captured() = ShellMetrics{};
    GuiTest::Harness harness(dpi);
    GuiTest::EnterReadyView("afp_modern", "sdvx7");
    GuiTest::LoadScene("bg_0001.ifs", 0, 300);

    ImGuiTest* test = harness.NewTest("dpi_shell_chrome");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        CheckChildFits(ctx, "topbar");
        CheckChildFits(ctx, "status_strip");
        CheckChildFits(ctx, "main_view/##timeline_dock");

        ShellMetrics& out = Captured();
        out.top_bar_h = ctx->WindowInfo("topbar").Window->Size.y;
        out.status_h = ctx->WindowInfo("status_strip").Window->Size.y;
        out.timeline_h = ctx->WindowInfo("main_view/##timeline_dock").Window->Size.y;
        out.text_h = ImGui::GetTextLineHeight();
    };
    harness.Run(test);
    return Captured();
}

}

TEST_CASE("the style and fonts scale with the display dpi", "[gui][dpi]") {
    unsigned const dpi = GENERATE(96U, 144U, 192U);
    INFO("display dpi " << dpi);
    GuiTest::Harness const harness(dpi);

    const float scale = (float)dpi / 96.0F;
    CHECK_THAT(Gui::Dpi::Scale(), Catch::Matchers::WithinAbs(scale, 0.001F));

    const ImGuiStyle& style = ImGui::GetStyle();
    CHECK_THAT(style.FontScaleDpi, Catch::Matchers::WithinAbs(scale, 0.001F));
    CHECK_THAT(style.FramePadding.x, Catch::Matchers::WithinAbs(9.0F * scale, 0.51F));
    CHECK_THAT(style.WindowPadding.y, Catch::Matchers::WithinAbs(10.0F * scale, 0.51F));
    CHECK_THAT(style.ScrollbarSize, Catch::Matchers::WithinAbs(12.0F * scale, 0.51F));
}

TEST_CASE("the shell chrome grows with the display dpi and still holds its content",
          "[gui][dpi][shell]") {
    const ShellMetrics base = MeasureShell(96);
    const ShellMetrics high = MeasureShell(192);

    CHECK_THAT(high.text_h, Catch::Matchers::WithinAbs(base.text_h * 2.0F, 1.0F));
    CHECK_THAT(high.top_bar_h, Catch::Matchers::WithinAbs(base.top_bar_h * 2.0F, 1.0F));
    CHECK_THAT(high.status_h, Catch::Matchers::WithinAbs(base.status_h * 2.0F, 1.0F));
    CHECK_THAT(high.timeline_h, Catch::Matchers::WithinAbs(base.timeline_h * 2.0F, 1.0F));
    CHECK_THAT(base.top_bar_h, Catch::Matchers::WithinAbs(Gui::kTopBarH, 1.0F));
}
