#include "gui_test_harness.h"

#include "editor/preset_editor_state.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/eval_resolve.h"
#include "preset/eval/eval_state.h"
#include "preset/eval/frame_report.h"
#include "preset/eval/frame_state.h"
#include "state/app_state.h"
#include "state/telemetry.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace Doc = Preset::Doc;

Doc::Document ConflictDocument() {
    Doc::Document document;
    document.id = "gui-frame-inspector";
    document.name = "Frame inspector";
    document.build = "iidx11";
    document.length = 600;

    Doc::Clip draw;
    draw.id = "core_draw";
    draw.start = 0;
    draw.end = 600;
    draw.command = Doc::ModelDraw{.asset = "scene", .model = "core", .alpha = 1.0};

    Doc::Clip fade;
    fade.id = "core_fade";
    fade.start = 100;
    fade.end = 200;
    fade.command = Doc::ModelTween{};
    fade.keys.push_back(Doc::Key{.at = 0, .values = {Doc::KeyValue{.id = "alpha", .value = 0.25}}});

    Doc::Track track;
    track.id = "core";
    track.name = "core";
    track.kind = Doc::TrackKind::Model;
    track.target = "core";
    track.clips.push_back(std::move(draw));
    track.clips.push_back(std::move(fade));
    document.tracks.push_back(std::move(track));
    return document;
}

void OpenFrameInspector(int frame) {
    GuiTest::EnterReadyView("scene3d", "iidx11");
    const Doc::Document document = ConflictDocument();
    const std::vector<int> choices;
    const Preset::Eval::EvalState runtime;
    const Preset::Eval::FrameState resolved = Preset::Eval::ResolveFrame(
        Preset::Eval::ResolveInput{.document = &document, .choices = &choices}, frame);

    App::PresetStatus status;
    status.id = document.id;
    status.frame = frame;
    status.length = 600;
    status.fps = 60;
    status.frame_report = std::make_shared<const Preset::Eval::FrameReport>(
        Preset::Eval::BuildFrameReport(document, resolved, runtime));
    App::Global().SetPresetStatus(std::move(status));

    Editor::Global().LoadDocument(document);
    while (App::Global().TakeCommand().has_value()) {
    }
}

}

TEST_CASE("the frame inspector tab names the clip that won a value", "[gui][frame_inspector]") {
    GuiTest::Harness harness;
    OpenFrameInspector(150);

    ImGuiTest* test = harness.NewTest("frame_inspector_winner");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        IM_CHECK(ctx->ItemExists("##inspector_tabs/Frame"));
        ctx->ItemClick("##inspector_tabs/Frame");
        ctx->Yield(2);
        const std::string text = GuiTest::CaptureFrameText(ctx);
        IM_CHECK(text.find("core_fade") != std::string::npos);
        IM_CHECK(text.find("0.25") != std::string::npos);
    };
    harness.Run(test);
}

TEST_CASE("clicking the winning clip in the frame inspector selects it", "[gui][frame_inspector]") {
    GuiTest::Harness harness;
    OpenFrameInspector(150);

    ImGuiTest* test = harness.NewTest("frame_inspector_select");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemClick("##inspector_tabs/Frame");
        ctx->Yield(2);
        ctx->ItemClick("**/###frame_win_core_alpha");
        ctx->Yield(2);
        IM_CHECK(Editor::Global().IsSelected("core_fade"));
    };
    harness.Run(test);
}
