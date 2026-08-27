#include "gui_test_harness.h"

#include "editor/preset_editor_state.h"
#include "editor/timeline_view.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "preset/asset_index.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "state/app_state.h"
#include "state/telemetry.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace Doc = Preset::Doc;

constexpr int kLength = 600;

Preset::AssetIndex StubIndex() {
    Preset::AssetIndex index;
    Preset::AssetEntry red;
    red.id = "red";
    red.kind = Doc::AssetKind::Scene3d;
    red.dir = "data/graph/model/red";
    red.loaded = true;
    red.models = {"core", "shield"};
    index.assets.push_back(std::move(red));
    return index;
}

Doc::Clip Clip(const std::string& id, int start, std::optional<int> end, Doc::Command command) {
    Doc::Clip clip;
    clip.id = id;
    clip.start = start;
    clip.end = end;
    clip.command = std::move(command);
    return clip;
}

Doc::Document MakeDocument() {
    Doc::Document document;
    document.id = "gui-modal-layout";
    document.name = "GUI modal layout";
    document.build = "iidx11";
    document.length = kLength;
    document.assets.push_back(
        Doc::Asset{.id = "red", .kind = Doc::AssetKind::Scene3d, .dir = "data/graph/model/red"});

    Doc::Track core;
    core.id = "core";
    core.name = "core";
    core.kind = Doc::TrackKind::Model;
    core.target = "ghost";
    core.clips.push_back(
        Clip("core_ghost", 0, 200, Doc::ModelDraw{.asset = "red", .model = "ghost"}));
    document.tracks.push_back(std::move(core));

    Doc::OptionSpec option;
    option.id = "mode";
    option.label = "Selected mode";
    option.choices.push_back(Doc::ChoiceSpec{.label = "BEGINNER"});
    option.choices.push_back(Doc::ChoiceSpec{.label = "7KEYS"});
    document.options.push_back(std::move(option));
    return document;
}

void Open(Doc::Document document) {
    GuiTest::EnterReadyView("scene3d", "iidx11");
    App::PresetStatus status;
    status.id = "gui-modal-layout";
    status.frame = 0;
    status.length = kLength;
    status.fps = 60;
    status.assets = std::make_shared<const Preset::AssetIndex>(StubIndex());
    App::Global().SetPresetStatus(std::move(status));

    Editor::State& editor = Editor::Global();
    editor.LoadDocument(std::move(document));
    editor.MutView().px_per_frame = 1.0;
    editor.MutView().snap = false;
    editor.ClearSelection();
    while (App::Global().TakeCommand().has_value()) {
    }
}

void FocusEditor(ImGuiTestContext* ctx) {
    ctx->SetRef("##main");
    GuiTest::FocusChild(ctx, "main_view/##timeline_editor");
}

void CheckFits(ImGuiTestContext* ctx, const char* path) {
    const ImGuiWindow* window = ctx->WindowInfo(path).Window;
    IM_CHECK_SILENT(window != nullptr);
    if (window->ContentSize.x > window->WorkRect.GetWidth()) {
        ctx->LogError("%s draws %.1f px of content into %.1f px of room", path,
                      window->ContentSize.x, window->WorkRect.GetWidth());
    }
    IM_CHECK_LE(window->ContentSize.x, window->WorkRect.GetWidth());
}

void CheckInside(ImGuiTestContext* ctx, const char* window_path, const char* item) {
    const ImGuiWindow* window = ctx->WindowInfo(window_path).Window;
    IM_CHECK_SILENT(window != nullptr);
    const ImGuiTestItemInfo info = ctx->ItemInfo(item);
    IM_CHECK_NE(info.ID, 0U);
    IM_CHECK_GE(info.RectFull.Min.x, window->InnerRect.Min.x);
    IM_CHECK_LE(info.RectFull.Max.x, window->InnerRect.Max.x);
}

void CheckText(ImGuiTestContext* ctx, const char* needle) {
    const std::string text = GuiTest::CaptureFrameText(ctx);
    if (text.find(needle) == std::string::npos) ctx->LogError("no \"%s\" on screen", needle);
    IM_CHECK(text.find(needle) != std::string::npos);
}

}

TEST_CASE("the Add track modal keeps its asset warning inside the modal",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeDocument());

    ImGuiTest* test = harness.NewTest("tl_modal_add_track_fits");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_add_track");
        ctx->Yield(3);
        ctx->SetRef("Add track");
        GuiTest::ComboPick(ctx, "###tl_track_kind", "sprite");
        ctx->Yield(3);

        CheckText(ctx, "asset not loaded");
        CheckFits(ctx, "//Add track");
        CheckInside(ctx, "//Add track", "###tl_track_target");
        CheckInside(ctx, "//Add track", "###tl_track_add");

        ctx->ItemClick("###tl_track_cancel");
        ctx->Yield(2);
    };
    harness.Run(test);
    Editor::Global().Close();
}

TEST_CASE("the clip properties modal keeps its not-loaded warning inside the modal",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeDocument());

    ImGuiTest* test = harness.NewTest("tl_modal_clip_fits");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->MouseMove("###tl_ruler");
        ctx->ItemDoubleClick("###tl_clip_core_ghost");
        ctx->Yield(6);
        GuiTest::FocusChild(ctx, "//Clip properties/###tl_clip_body");
        ctx->ItemClick("###tl_clip_tabs/###tl_tab_params");
        ctx->Yield(3);

        CheckText(ctx, "not in the loaded asset");
        CheckFits(ctx, "//Clip properties");
        CheckFits(ctx, "//Clip properties/###tl_clip_body");
        CheckInside(ctx, "//Clip properties", "###tl_clip_tabs/###tl_tab_params/###tl_field_model");

        ctx->ItemClick("###tl_clip_tabs/###tl_tab_asset");
        ctx->Yield(3);
        CheckFits(ctx, "//Clip properties");
        CheckFits(ctx, "//Clip properties/###tl_clip_body");

        ctx->SetRef("Clip properties");
        ctx->ItemClick("###tl_clip_cancel");
        ctx->Yield(2);
    };
    harness.Run(test);
    Editor::Global().Close();
}

TEST_CASE("the document and option properties modals keep every hint inside the modal",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeDocument());

    ImGuiTest* document = harness.NewTest("tl_modal_doc_fits");
    document->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_doc_badge");
        ctx->Yield(3);
        ctx->SetRef("Document properties");
        for (const char* tab : {"###tl_doc_tab_general", "###tl_doc_tab_render",
                                "###tl_doc_tab_camera", "###tl_doc_tab_lights"}) {
            ctx->ItemClick((std::string("###tl_doc_tabs/") + tab).c_str());
            ctx->Yield(3);
            CheckFits(ctx, "//Document properties");
        }
        ctx->ItemClick("###tl_doc_cancel");
        ctx->Yield(2);
    };
    harness.Run(document);

    ImGuiTest* option = harness.NewTest("tl_modal_option_fits");
    option->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_opt_edit_mode");
        ctx->Yield(3);
        CheckFits(ctx, "//Option properties");
        ctx->SetRef("Option properties");
        ctx->ItemClick("###tl_option_cancel");
        ctx->Yield(2);
    };
    harness.Run(option);
    Editor::Global().Close();
}
