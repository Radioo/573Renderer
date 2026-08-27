#include "gui_test_harness.h"

#include "editor/clip_problems.h"
#include "editor/preset_editor_state.h"
#include "editor/timeline_edits.h"
#include "editor/timeline_view.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "preset/asset_index.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_fields.h"
#include "preset/doc/preset_validate.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "state/preset_commands.h"
#include "state/telemetry.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <any>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
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

    Preset::AssetEntry title;
    title.id = "title";
    title.kind = Doc::AssetKind::Package2d;
    title.dir = "data/graph/sys/title";
    title.loaded = true;
    title.cells = {"OP_BG_D", "OP_BG_U"};
    title.animations.push_back(Preset::AssetAnimation{
        .name = "TITLE", .frames = 1736, .parts = {"TITLE_TAIKI", "OP_BG_U"}});
    title.animations.push_back(
        Preset::AssetAnimation{.name = "TITLE_TAIKI", .frames = 720, .parts = {}});
    index.assets.push_back(std::move(title));
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

Doc::Track Track(const std::string& id, Doc::TrackKind kind, std::vector<Doc::Clip> clips) {
    Doc::Track track;
    track.id = id;
    track.name = id;
    track.kind = kind;
    track.target = id;
    track.clips = std::move(clips);
    return track;
}

Doc::Document MakeDocument() {
    Doc::Document document;
    document.id = "gui-modal-test";
    document.name = "GUI modal test";
    document.build = "iidx11";
    document.length = kLength;
    document.assets.push_back(
        Doc::Asset{.id = "red", .kind = Doc::AssetKind::Scene3d, .dir = "data/graph/model/red"});
    document.assets.push_back(Doc::Asset{
        .id = "title", .kind = Doc::AssetKind::Package2d, .dir = "data/graph/sys/title"});
    document.tracks.push_back(
        Track("core", Doc::TrackKind::Model,
              {Clip("core_warp", 0, 200, Doc::ModelDraw{.asset = "red", .model = "core"})}));
    document.tracks.push_back(
        Track("TITLE", Doc::TrackKind::Sprite,
              {Clip("title_boot", 0, 400,
                    Doc::SpriteAnimate{.asset = "title", .animation = "TITLE", .priority = 15})}));
    return document;
}

Doc::Document MakeModeSelectDocument() {
    Doc::Document document = MakeDocument();
    document.id = "gui-modal-test";
    Doc::OptionSpec option;
    option.id = "mode";
    option.label = "Selected mode";
    option.choices.push_back(Doc::ChoiceSpec{.label = "BEGINNER"});
    option.choices.push_back(Doc::ChoiceSpec{.label = "7KEYS"});
    option.choices.push_back(Doc::ChoiceSpec{.label = "CLASS COURSE"});
    document.options.push_back(std::move(option));
    return document;
}

void Open(Doc::Document document, int frame) {
    GuiTest::EnterReadyView("scene3d", "iidx11");
    App::PresetStatus status;
    status.id = "gui-modal-test";
    status.frame = frame;
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

std::vector<PresetCmd::Any> DrainPresetCommands() {
    std::vector<PresetCmd::Any> out;
    while (const std::optional<App::Command> command = App::Global().TakeCommand()) {
        const auto* backend = std::get_if<App::Cmd::BackendCommand>(&*command);
        if (backend == nullptr) continue;
        const auto* preset = std::any_cast<PresetCmd::Any>(&backend->payload);
        if (preset != nullptr) out.push_back(*preset);
    }
    return out;
}

const Doc::Clip* Find(const std::string& id) {
    const Doc::Document& document = Editor::Global().Document();
    const Editor::ClipRef ref = Editor::FindClip(document, id);
    if (!ref.Valid()) return nullptr;
    return &document.tracks[(std::size_t)ref.track].clips[(std::size_t)ref.clip];
}

int g_expected_errors = 0;
int g_undo_before_drag = 0;
float g_problem_row_h = 0.0F;
float g_problem_line_h = 0.0F;
std::string g_early_tooltip;
std::string g_late_tooltip;
std::string g_params_text;
float g_footer_bottom = 0.0F;
float g_window_bottom = 0.0F;

bool Tweenable(Doc::CommandType type) {
    return std::ranges::any_of(Doc::KeyFieldsFor(type),
                               [](const Doc::FieldDesc& field) { return field.tweenable; });
}
double g_alpha_during_edit = 0.0;
bool g_replace_during_edit = false;

void FocusEditor(ImGuiTestContext* ctx) {
    ctx->SetRef("##main");
    GuiTest::FocusChild(ctx, "main_view/##timeline_editor");
}

void CheckFormFields(ImGuiTestContext* ctx, Doc::CommandType type) {
    for (const Doc::FieldDesc& field : Doc::FieldsFor(type)) {
        const std::string item =
            "###tl_clip_tabs/###tl_tab_params/###tl_field_" + std::string(field.id);
        const std::string axis = item + "_x";
        const bool present = ctx->ItemExists(item.c_str()) || ctx->ItemExists(axis.c_str());
        if (!present) ctx->LogError("missing %s", item.c_str());
        IM_CHECK_SILENT(present);
    }
}

std::string TrackIdFor(Doc::CommandType type) {
    std::string id;
    for (const char c : Doc::kCommandTypeNames[(std::size_t)type])
        id += (c == '.') ? '_' : c;
    return id;
}

void ClipBody(ImGuiTestContext* ctx) {
    ctx->SetRef("Clip properties");
    GuiTest::FocusChild(ctx, "//Clip properties/###tl_clip_body");
}

void OpenClipModal(ImGuiTestContext* ctx, const char* clip_id) {
    ctx->Yield(4);
    FocusEditor(ctx);
    ctx->MouseMove("###tl_ruler");
    ctx->ItemDoubleClick((std::string("###tl_clip_") + clip_id).c_str());
    ctx->Yield(6);
}

}

TEST_CASE("the A key opens the command palette on the selected track", "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeDocument(), 300);

    ImGuiTest* test = harness.NewTest("tl_palette_open");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_clip_core_warp");
        ctx->Yield(2);
        ctx->MouseMove("###tl_ruler");
        ctx->KeyPress(ImGuiKey_A);
        ctx->Yield(3);
        ctx->SetRef("Add command");
        IM_CHECK(ctx->ItemExists("###tl_palette_filter"));
        IM_CHECK(ctx->ItemExists("###tl_palette_model.tween"));
        IM_CHECK(ctx->ItemExists("###tl_palette_emitter"));
        ctx->ItemClick("###tl_palette_cancel");
        ctx->Yield(2);
    };
    harness.Run(test);
    Editor::Global().Close();
}

TEST_CASE("filtering to tw and pressing Enter inserts a model tween and opens its modal",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeDocument(), 300);

    ImGuiTest* test = harness.NewTest("tl_palette_insert");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_clip_core_warp");
        ctx->Yield(2);
        ctx->ItemClick("###tl_add_command");
        ctx->Yield(3);
        ctx->SetRef("Add command");
        ctx->ItemInput("###tl_palette_filter");
        ctx->KeyChars("tw");
        ctx->Yield(2);
        ctx->KeyPress(ImGuiKey_Enter);
        ctx->Yield(4);
        ClipBody(ctx);
        ctx->ItemClick("//Clip properties/###tl_clip_done");
        ctx->Yield(2);
    };
    harness.Run(test);

    const Doc::Document& document = Editor::Global().Document();
    REQUIRE(document.tracks[0].clips.size() == 2);
    const Doc::Clip& added = document.tracks[0].clips[1];
    CHECK(Doc::TypeOf(added.command) == Doc::CommandType::ModelTween);
    CHECK(added.start == 300);
    CHECK(Editor::Global().IsSelected(added.id));
    Editor::Global().Close();
}

TEST_CASE("every catalog command shows a widget for every one of its field descriptors",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Doc::Document document = MakeDocument();
    document.tracks.clear();
    for (std::size_t i = 0; i < Doc::kCommandTypeNames.size(); i++) {
        const auto type = (Doc::CommandType)i;
        const std::string id = TrackIdFor(type);
        document.tracks.push_back(
            Track(id, Doc::TraitsFor(type).kind,
                  {Clip(id + "_clip", 0,
                        Doc::IsEvent(type) ? std::optional<int>{} : std::optional<int>{100},
                        Doc::DefaultCommand(type))}));
    }
    Open(std::move(document), 0);
    Editor::Global().MutView().height = 620.0F;

    ImGuiTest* test = harness.NewTest("tl_forms_cover_catalog");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        for (std::size_t i = 0; i < Doc::kCommandTypeNames.size(); i++) {
            const auto type = (Doc::CommandType)i;
            if (Doc::FieldsFor(type).empty()) continue;
            Editor::Global().MutView().track_scroll =
                (i * 2 < Doc::kCommandTypeNames.size()) ? 0.0F : 100000.0F;
            ctx->Yield(2);
            OpenClipModal(ctx, (TrackIdFor(type) + "_clip").c_str());
            ClipBody(ctx);
            ctx->ItemClick("###tl_clip_tabs/###tl_tab_params");
            ctx->Yield(2);
            CheckFormFields(ctx, type);
            ctx->ItemClick("//Clip properties/###tl_clip_cancel");
            ctx->Yield(2);
        }
    };
    harness.Run(test);
    Editor::Global().Close();
}

TEST_CASE("editing a field applies live and Cancel restores the document",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeDocument(), 0);
    g_alpha_during_edit = 0.0;
    g_replace_during_edit = false;

    ImGuiTest* test = harness.NewTest("tl_modal_edit_cancel");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenClipModal(ctx, "core_warp");
        ClipBody(ctx);
        ctx->ItemClick("###tl_clip_tabs/###tl_tab_params");
        ctx->ItemInputValue("###tl_clip_tabs/###tl_tab_params/###tl_field_alpha", 0.25F);
        ctx->Yield(3);
        g_alpha_during_edit = std::get<Doc::ModelDraw>(Find("core_warp")->command).alpha;
        for (const PresetCmd::Any& command : DrainPresetCommands()) {
            g_replace_during_edit = g_replace_during_edit ||
                                    std::holds_alternative<PresetCmd::ReplaceDocument>(command);
        }
        ClipBody(ctx);
        ctx->ItemClick("//Clip properties/###tl_clip_cancel");
        ctx->Yield(3);
    };
    harness.Run(test);

    CHECK(g_alpha_during_edit == 0.25);
    CHECK(g_replace_during_edit);
    REQUIRE(Find("core_warp") != nullptr);
    CHECK(std::get<Doc::ModelDraw>(Find("core_warp")->command).alpha == 1.0);
    Editor::Global().Close();
}

TEST_CASE("Ctrl+Z inside the modal undoes one field at a time", "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeDocument(), 0);

    ImGuiTest* test = harness.NewTest("tl_modal_undo");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenClipModal(ctx, "core_warp");
        ClipBody(ctx);
        ctx->ItemClick("###tl_clip_tabs/###tl_tab_params");
        ctx->ItemInputValue("###tl_clip_tabs/###tl_tab_params/###tl_field_alpha", 0.5F);
        ctx->Yield(2);
        ctx->ItemInputValue("###tl_clip_tabs/###tl_tab_params/###tl_field_anim_speed", 0.75F);
        ctx->Yield(2);
        ctx->KeyDown(ImGuiKey_LeftCtrl);
        ctx->KeyPress(ImGuiKey_Z);
        ctx->KeyUp(ImGuiKey_LeftCtrl);
        ctx->Yield(3);
    };
    harness.Run(test);

    const auto& draw = std::get<Doc::ModelDraw>(Find("core_warp")->command);
    CHECK(draw.anim_speed == 1.0);
    CHECK(draw.alpha == 0.5);
    Editor::Global().Close();
}

TEST_CASE("the asset combos list exactly the names the published index carries",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeDocument(), 0);

    ImGuiTest* test = harness.NewTest("tl_asset_picker");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenClipModal(ctx, "title_boot");
        ClipBody(ctx);
        ctx->ItemClick("###tl_clip_tabs/###tl_tab_params");
        ctx->Yield(2);
        GuiTest::ComboPick(ctx, "###tl_clip_tabs/###tl_tab_params/###tl_field_animation",
                           "TITLE_TAIKI");
        ctx->Yield(2);
        ctx->ItemClick("###tl_clip_tabs/###tl_tab_asset");
        ctx->Yield(2);
        IM_CHECK(ctx->ItemExists("###tl_clip_tabs/###tl_tab_asset/###tl_asset_dir"));
        ClipBody(ctx);
        ctx->ItemClick("//Clip properties/###tl_clip_done");
        ctx->Yield(2);
    };
    harness.Run(test);

    CHECK(std::get<Doc::SpriteAnimate>(Find("title_boot")->command).animation == "TITLE_TAIKI");
    Editor::Global().Close();
}

TEST_CASE("the Add track modal offers the index model names and inserts the track",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeDocument(), 0);

    ImGuiTest* test = harness.NewTest("tl_add_track");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_add_track");
        ctx->Yield(3);
        ctx->SetRef("Add track");
        GuiTest::ComboPick(ctx, "###tl_track_target", "shield");
        ctx->Yield(2);
        ctx->ItemClick("###tl_track_add");
        ctx->Yield(3);
    };

    harness.Run(test);

    const Doc::Document& document = Editor::Global().Document();
    REQUIRE(document.tracks.size() == 3);
    const auto added = std::ranges::find_if(
        document.tracks, [](const Doc::Track& track) { return track.target == "shield"; });
    REQUIRE(added != document.tracks.end());
    CHECK(added->kind == Doc::TrackKind::Model);
    CHECK(added->clips.empty());
    Editor::Global().Close();
}

TEST_CASE("the document properties modal edits the render size and the default camera",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeDocument(), 0);

    ImGuiTest* test = harness.NewTest("tl_doc_modal");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_doc_badge");
        ctx->Yield(3);
        ctx->SetRef("Document properties");
        ctx->ItemClick("###tl_doc_tabs/###tl_doc_tab_render");
        ctx->ItemInputValue("###tl_doc_tabs/###tl_doc_tab_render/###tl_doc_width", 1280);
        ctx->Yield(2);
        ctx->ItemClick("###tl_doc_tabs/###tl_doc_tab_camera");
        ctx->ItemInputValue("###tl_doc_tabs/###tl_doc_tab_camera/###tl_doc_eye_x", 2.5F);
        ctx->Yield(2);
        ctx->SetRef("Document properties");
        ctx->ItemClick("###tl_doc_done");
        ctx->Yield(3);
    };
    harness.Run(test);

    const Doc::Document& document = Editor::Global().Document();
    CHECK(document.render.width == 1280);
    CHECK(document.camera.eye[0] == 2.5);
    Editor::Global().Close();
}

TEST_CASE("the hidden parts checklist writes hidden_parts with the docs verdict beside it",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeDocument(), 0);

    ImGuiTest* test = harness.NewTest("tl_hidden_parts");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenClipModal(ctx, "title_boot");
        ClipBody(ctx);
        ctx->ItemClick("###tl_clip_tabs/###tl_tab_params");
        ctx->Yield(2);
        IM_CHECK(ctx->ItemExists("###tl_clip_tabs/###tl_tab_params/###tl_hidden_OP_BG_U"));
        const std::string text = GuiTest::CaptureFrameText(ctx);
        IM_CHECK(text.find("chrome") != std::string::npos);
        ctx->ItemClick("###tl_clip_tabs/###tl_tab_params/###tl_hidden_OP_BG_U");
        ctx->Yield(2);
        ClipBody(ctx);
        ctx->ItemClick("//Clip properties/###tl_clip_done");
        ctx->Yield(2);
    };
    harness.Run(test);

    const auto& animate = std::get<Doc::SpriteAnimate>(Find("title_boot")->command);
    REQUIRE(animate.hidden_parts.size() == 1);
    CHECK(animate.hidden_parts[0] == "OP_BG_U");
    Editor::Global().Close();
}

TEST_CASE("the preview strip asks for one PreviewLayer per cache key and shows its progress",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeDocument(), 0);

    ImGuiTest* test = harness.NewTest("tl_preview_strip");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenClipModal(ctx, "title_boot");
        ClipBody(ctx);
        ctx->ItemClick("###tl_clip_tabs/###tl_tab_params");
        ctx->Yield(6);
        const std::string text = GuiTest::CaptureFrameText(ctx);
        IM_CHECK(text.find("sample") != std::string::npos);
        ClipBody(ctx);
        ctx->ItemClick("//Clip properties/###tl_clip_cancel");
        ctx->Yield(2);
    };
    harness.Run(test);

    int requests = 0;
    std::string animation;
    for (const PresetCmd::Any& command : DrainPresetCommands()) {
        const auto* preview = std::get_if<PresetCmd::PreviewLayer>(&command);
        if (preview == nullptr) continue;
        requests++;
        animation = preview->animation;
    }
    CHECK(requests == 1);
    CHECK(animation == "TITLE");
    Editor::Global().Close();
}

TEST_CASE("the validation badge counts what Validate reports and jumps to the clip",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Doc::Document document = MakeDocument();
    document.tracks[0].clips.push_back(
        Clip("core_bad", 500, 400, Doc::ModelDraw{.asset = "red", .model = "core"}));
    const int errors =
        (int)std::ranges::count_if(Doc::Validate(document), [](const Doc::Problem& problem) {
            return problem.severity == Doc::Severity::Error;
        });
    REQUIRE(errors > 0);
    g_expected_errors = errors;
    Open(std::move(document), 0);

    ImGuiTest* test = harness.NewTest("tl_validation_badge");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        const std::string badge = GuiTest::HoverAndCaptureText(ctx, "###tl_problems");
        IM_CHECK(badge.find(std::to_string(g_expected_errors)) != std::string::npos);
        ctx->ItemClick("###tl_problems");
        ctx->Yield(3);
        ctx->SetRef("Problems");
        ctx->ItemClick("###tl_problem_0");
        ctx->Yield(3);
    };
    harness.Run(test);

    CHECK(Editor::Global().IsSelected("core_bad"));
    Editor::Global().Close();
}

TEST_CASE("the when gate offers the document's options, match kinds and choices",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeModeSelectDocument(), 0);

    ImGuiTest* test = harness.NewTest("tl_when_gate");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenClipModal(ctx, "core_warp");
        ClipBody(ctx);
        ctx->ItemClick("###tl_clip_tabs/###tl_tab_general");
        ctx->Yield(2);
        GuiTest::ComboPick(ctx, "###tl_clip_tabs/###tl_tab_general/###tl_general_gate_option",
                           "mode");
        ctx->Yield(2);
        GuiTest::ComboPick(ctx, "###tl_clip_tabs/###tl_tab_general/###tl_general_gate_kind",
                           "is not");
        ctx->Yield(2);
        ctx->ItemClick("###tl_clip_tabs/###tl_tab_general/###tl_general_gate_choice_7KEYS");
        ctx->Yield(2);
        ClipBody(ctx);
        ctx->ItemClick("//Clip properties/###tl_clip_done");
        ctx->Yield(2);
    };
    harness.Run(test);

    REQUIRE(Find("core_warp") != nullptr);
    const std::optional<Doc::Gate>& stored = Find("core_warp")->when;
    REQUIRE(stored.has_value());
    const Doc::Gate gate = stored.value_or(Doc::Gate{});
    CHECK(gate.option == "mode");
    CHECK(gate.kind == Doc::GateKind::Not);
    REQUIRE(gate.choices.size() == 1);
    CHECK(gate.choices[0] == "7KEYS");
    Editor::Global().Close();
}

TEST_CASE("the palette lists Select option under Document and inserts it as an event clip",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeModeSelectDocument(), 300);

    ImGuiTest* test = harness.NewTest("tl_option_select_insert");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_add_command");
        ctx->Yield(3);
        ctx->SetRef("Add command");
        IM_CHECK(ctx->ItemExists("###tl_palette_option.select"));
        ctx->ItemClick("###tl_palette_option.select");
        ctx->Yield(4);
        ClipBody(ctx);
        ctx->ItemClick("//Clip properties/###tl_clip_done");
        ctx->Yield(2);
    };
    harness.Run(test);

    const Doc::Document& document = Editor::Global().Document();
    const auto track = std::ranges::find_if(
        document.tracks, [](const Doc::Track& t) { return t.kind == Doc::TrackKind::Scene; });
    REQUIRE(track != document.tracks.end());
    REQUIRE(track->clips.size() == 1);
    CHECK(Doc::TypeOf(track->clips[0].command) == Doc::CommandType::OptionSelect);
    CHECK(track->clips[0].start == 300);
    CHECK_FALSE(track->clips[0].end.has_value());
    Editor::Global().Close();
}

TEST_CASE("Esc closes the clip modal and restores it the way Cancel does",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeDocument(), 0);

    ImGuiTest* test = harness.NewTest("tl_modal_escape");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenClipModal(ctx, "core_warp");
        ClipBody(ctx);
        ctx->ItemClick("###tl_clip_tabs/###tl_tab_params");
        ctx->ItemInputValue("###tl_clip_tabs/###tl_tab_params/###tl_field_alpha", 0.25F);
        ctx->Yield(3);
        ctx->KeyPress(ImGuiKey_Escape);
        ctx->Yield(4);
        ctx->SetRef("");
        IM_CHECK_SILENT(ctx->ItemExists("//Clip properties/###tl_clip_done") == false);
    };
    harness.Run(test);

    REQUIRE(Find("core_warp") != nullptr);
    CHECK(std::get<Doc::ModelDraw>(Find("core_warp")->command).alpha == 1.0);
    Editor::Global().Close();
}

TEST_CASE("a drag in the Clip inspector tab is one undo entry, not one per frame",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeDocument(), 0);

    ImGuiTest* test = harness.NewTest("tl_clip_tab_drag");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_clip_core_warp");
        ctx->Yield(2);
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemClick("##inspector_tabs/Clip");
        ctx->Yield(2);
        g_undo_before_drag = Editor::Global().UndoDepth();
        ctx->ItemDragWithDelta("##inspector_tabs/Clip/###tl_field_alpha", ImVec2(-40.0F, 0.0F));
        ctx->Yield(3);
    };
    harness.Run(test);

    const auto& draw = std::get<Doc::ModelDraw>(Find("core_warp")->command);
    CHECK(draw.alpha < 1.0);
    CHECK(Editor::Global().UndoDepth() == g_undo_before_drag + 1);
    Editor::Global().Close();
}

TEST_CASE("a long problem message wraps instead of being cut off by the popup edge",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Doc::Document document = MakeDocument();
    document.tracks[0].clips[0].id = "core_warp_in_rotating_and_zooming_towards_the_camera";
    document.tracks[0].clips.push_back(Clip("core_attract_loop_settled_to_the_right_of_the_screen",
                                            100, 300,
                                            Doc::ModelDraw{.asset = "red", .model = "core"}));
    REQUIRE_FALSE(Doc::Validate(document).empty());
    Open(std::move(document), 0);

    ImGuiTest* test = harness.NewTest("tl_problem_wraps");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_problems");
        ctx->Yield(3);
        ctx->SetRef("Problems");
        const ImGuiTestItemInfo info = ctx->ItemInfo("###tl_problem_0");
        IM_CHECK_SILENT(info.ID != 0);
        g_problem_row_h = info.RectFull.Max.y - info.RectFull.Min.y;
        g_problem_line_h = ImGui::GetTextLineHeightWithSpacing();
        ctx->ItemClick("###tl_problems_close");
        ctx->Yield(2);
    };
    harness.Run(test);

    CHECK(g_problem_row_h > g_problem_line_h);
    Editor::Global().Close();
}

TEST_CASE("clips with an overlap both carry a validation bar and name it on hover",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Doc::Document document = MakeDocument();
    document.tracks[0].clips.push_back(
        Clip("core_late", 180, 400, Doc::ModelDraw{.asset = "red", .model = "core"}));
    REQUIRE(Editor::ProblemsForClip(Doc::Validate(document), "core_warp").Failing());
    REQUIRE(Editor::ProblemsForClip(Doc::Validate(document), "core_late").Failing());
    Open(std::move(document), 0);

    ImGuiTest* test = harness.NewTest("tl_clip_problem_bar");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        g_early_tooltip = GuiTest::HoverAndCaptureText(ctx, "###tl_clip_core_warp");
        g_late_tooltip = GuiTest::HoverAndCaptureText(ctx, "###tl_clip_core_late");
    };
    harness.Run(test);

    CHECK(g_early_tooltip.find("error:") != std::string::npos);
    CHECK(g_early_tooltip.find("overlaps") != std::string::npos);
    CHECK(g_late_tooltip.find("error:") != std::string::npos);
    CHECK(g_late_tooltip.find("overlaps") != std::string::npos);
    Editor::Global().Close();
}

TEST_CASE("the Tween tab is hidden for a command that can carry no keys",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Doc::Document document = MakeDocument();
    document.tracks.push_back(
        Track("scene", Doc::TrackKind::Scene, {Clip("seed", 10, std::nullopt, Doc::RngSeed{})}));
    REQUIRE_FALSE(Tweenable(Doc::CommandType::RngSeed));
    REQUIRE(Tweenable(Doc::CommandType::ModelDraw));
    Open(std::move(document), 0);

    ImGuiTest* test = harness.NewTest("tl_tween_tab_hidden");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenClipModal(ctx, "core_warp");
        ClipBody(ctx);
        IM_CHECK(ctx->ItemExists("###tl_clip_tabs/###tl_tab_tween"));
        ctx->ItemClick("//Clip properties/###tl_clip_cancel");
        ctx->Yield(3);

        OpenClipModal(ctx, "seed");
        ClipBody(ctx);
        IM_CHECK(ctx->ItemExists("###tl_clip_tabs/###tl_tab_params"));
        IM_CHECK(ctx->ItemExists("###tl_clip_tabs/###tl_tab_tween") == false);
        ctx->ItemClick("//Clip properties/###tl_clip_cancel");
        ctx->Yield(2);
    };
    harness.Run(test);
    Editor::Global().Close();
}

TEST_CASE("a vec3 row prints its unit and an empty model names the track target",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Doc::Document document = MakeDocument();
    std::get<Doc::ModelDraw>(document.tracks[0].clips[0].command).model.clear();
    Open(std::move(document), 0);

    ImGuiTest* test = harness.NewTest("tl_units_and_target");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenClipModal(ctx, "core_warp");
        ClipBody(ctx);
        ctx->ItemClick("###tl_clip_tabs/###tl_tab_params");
        ctx->Yield(2);
        g_params_text = GuiTest::CaptureFrameText(ctx);
        ctx->ItemClick("//Clip properties/###tl_clip_cancel");
        ctx->Yield(2);
    };
    harness.Run(test);

    const Doc::FieldDesc* position =
        Doc::FindField(Doc::FieldsFor(Doc::CommandType::ModelDraw), "position");
    REQUIRE(position != nullptr);
    REQUIRE_FALSE(position->unit.empty());
    CHECK(g_params_text.find("position  " + std::string(position->unit)) != std::string::npos);
    CHECK(g_params_text.find("core (track target)") != std::string::npos);
    Editor::Global().Close();
}

TEST_CASE("the Clip inspector tab edits the label, extent and mute inline",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeModeSelectDocument(), 0);
    Editor::Global().Apply([](Doc::Document& document) {
        document.tracks[0].clips[0].when =
            Doc::Gate{.option = "mode", .kind = Doc::GateKind::Not, .choices = {"7KEYS"}};
        return true;
    });

    ImGuiTest* test = harness.NewTest("tl_clip_tab_inline");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_clip_core_warp");
        ctx->Yield(2);
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemClick("##inspector_tabs/Clip");
        ctx->Yield(2);
        g_params_text = GuiTest::CaptureFrameText(ctx);
        ctx->ItemInputValue("##inspector_tabs/Clip/###insp_clip_start", 40);
        ctx->Yield(2);
        ctx->ItemClick("##inspector_tabs/Clip/###insp_clip_mute");
        ctx->Yield(2);
    };
    harness.Run(test);

    CHECK(g_params_text.find("mode is not 7KEYS") != std::string::npos);
    REQUIRE(Find("core_warp") != nullptr);
    CHECK(Find("core_warp")->start == 40);
    CHECK(Find("core_warp")->muted);
    Editor::Global().Close();
}

TEST_CASE("the palette moves the highlight with the arrows and inserts the highlighted entry",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeDocument(), 300);

    ImGuiTest* test = harness.NewTest("tl_palette_arrows");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_clip_core_warp");
        ctx->Yield(2);
        ctx->ItemClick("###tl_add_command");
        ctx->Yield(3);
        ctx->SetRef("Add command");
        ctx->KeyPress(ImGuiKey_DownArrow);
        ctx->Yield(2);
        ctx->KeyPress(ImGuiKey_DownArrow);
        ctx->Yield(2);
        ctx->KeyPress(ImGuiKey_Enter);
        ctx->Yield(4);
        ClipBody(ctx);
        ctx->ItemClick("//Clip properties/###tl_clip_done");
        ctx->Yield(2);
    };
    harness.Run(test);

    const Doc::Document& document = Editor::Global().Document();
    REQUIRE(document.tracks[0].clips.size() == 2);
    CHECK(Doc::TypeOf(document.tracks[0].clips[1].command) == Doc::CommandType::ModelMotion);
    Editor::Global().Close();
}

TEST_CASE("the modal footer stays reachable however tall the tab content is",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Doc::Document document = MakeDocument();
    Open(std::move(document), 0);

    App::PresetStatus status = App::Global().GetPresetStatus();
    auto index = std::make_shared<Preset::AssetIndex>(*status.assets);
    std::vector<std::string> many;
    many.reserve(80);
    for (int i = 0; i < 80; i++)
        many.push_back("PART_" + std::to_string(i));
    index->assets[1].animations[0].parts = std::move(many);
    status.assets = index;
    App::Global().SetPresetStatus(std::move(status));

    ImGuiTest* test = harness.NewTest("tl_modal_footer");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        OpenClipModal(ctx, "title_boot");
        ClipBody(ctx);
        ctx->ItemClick("###tl_clip_tabs/###tl_tab_params");
        ctx->Yield(4);
        const ImGuiTestItemInfo done = ctx->ItemInfo("//Clip properties/###tl_clip_done");
        IM_CHECK_SILENT(done.ID != 0);
        const ImGuiWindow* window = ctx->WindowInfo("//Clip properties").Window;
        IM_CHECK_SILENT(window != nullptr);
        g_footer_bottom = done.RectFull.Max.y;
        g_window_bottom = window->InnerRect.Max.y;
        ctx->ItemClick("//Clip properties/###tl_clip_cancel");
        ctx->Yield(2);
    };
    harness.Run(test);

    CHECK(g_footer_bottom > 0.0F);
    CHECK(g_footer_bottom <= g_window_bottom);
    Editor::Global().Close();
}

TEST_CASE("the Clip inspector tab reports the selection and opens the modal",
          "[gui][timeline][modal]") {
    GuiTest::Harness harness;
    Open(MakeDocument(), 0);

    ImGuiTest* empty = harness.NewTest("tl_clip_tab_empty");
    empty->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        ctx->ItemClick("##inspector_tabs/Clip");
        ctx->Yield(2);
        const std::string text = GuiTest::CaptureFrameText(ctx);
        IM_CHECK(text.find("no clip selected") != std::string::npos);
    };
    harness.Run(empty);

    ImGuiTest* selected = harness.NewTest("tl_clip_tab_selected");
    selected->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_clip_core_warp");
        ctx->Yield(2);
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/pane_right");
        const std::string text = GuiTest::CaptureFrameText(ctx);
        IM_CHECK(text.find("core_warp") != std::string::npos);
        ctx->ItemClick("##inspector_tabs/Clip/###insp_clip_props");
        ctx->Yield(3);
        IM_CHECK(ctx->WindowInfo("//Clip properties").Window != nullptr);
        ClipBody(ctx);
        ctx->ItemClick("//Clip properties/###tl_clip_cancel");
        ctx->Yield(2);
    };
    harness.Run(selected);
    Editor::Global().Close();
}
