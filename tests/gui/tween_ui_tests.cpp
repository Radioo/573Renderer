#include "gui_test_harness.h"

#include "editor/preset_editor_state.h"
#include "editor/timeline_edits.h"
#include "editor/tween_edits.h"
#include "imgui.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/eval_resolve.h"
#include "preset/eval/frame_state.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "state/preset_commands.h"
#include "state/telemetry.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <any>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace Doc = Preset::Doc;

constexpr int kLength = 600;

Doc::Clip Draw(const std::string& id, int start, std::optional<int> end) {
    Doc::Clip clip;
    clip.id = id;
    clip.start = start;
    clip.end = end;
    clip.command = Doc::ModelDraw{.asset = "scene", .model = "core", .alpha = 0.25};
    return clip;
}

Doc::Track ModelTrack(const std::string& id, std::vector<Doc::Clip> clips) {
    Doc::Track track;
    track.id = id;
    track.name = id;
    track.kind = Doc::TrackKind::Model;
    track.target = "core";
    track.clips = std::move(clips);
    return track;
}

Doc::Document MakeDocument() {
    Doc::Document document;
    document.id = "tween-ui-test";
    document.name = "Tween UI test";
    document.build = "iidx11";
    document.length = kLength;
    document.assets.push_back(
        Doc::Asset{.id = "scene", .kind = Doc::AssetKind::Scene3d, .dir = "data/model"});
    document.tracks.push_back(
        ModelTrack("core", {Draw("core_a", 0, 200), Draw("core_b", 200, 400)}));

    Doc::Clip tween;
    tween.id = "core_fade";
    tween.start = 100;
    tween.end = 200;
    tween.command = Doc::ModelTween{};
    tween.keys.push_back(Doc::Key{.at = 0, .values = {Doc::KeyValue{.id = "alpha", .value = 0.0}}});
    tween.keys.push_back(
        Doc::Key{.at = 100, .values = {Doc::KeyValue{.id = "alpha", .value = 1.0}}});
    document.tracks.push_back(ModelTrack("core_tween", {std::move(tween)}));
    return document;
}

Doc::Document MakeOptionDocument() {
    Doc::Document document = MakeDocument();
    document.options.push_back(Doc::OptionSpec{
        .id = "mode",
        .label = "Selected mode",
        .default_choice = 0,
        .transition = {.frames = 100, .step = 4, .spin_kick = 15.0},
        .choices = {
            Doc::ChoiceSpec{.label = "BEGINNER",
                            .values = {Doc::ChoiceValue{.id = "model[core].position",
                                                        .value = Doc::Vec3{1.0, 0.0, 0.0}}}},
            Doc::ChoiceSpec{.label = "EXPERT",
                            .values = {Doc::ChoiceValue{.id = "model[core].position",
                                                        .value = Doc::Vec3{2.0, 0.0, 0.0}}}},
            Doc::ChoiceSpec{.label = "FREE"}}});
    return document;
}

void OpenDocument(Doc::Document document, int frame, std::vector<int> choices) {
    GuiTest::EnterReadyView("scene3d", "iidx11");
    App::PresetStatus status;
    status.id = document.id;
    status.frame = frame;
    status.length = kLength;
    status.fps = 60;
    status.playing = false;
    status.loop = true;
    status.option_choices = std::move(choices);
    App::Global().SetPresetStatus(std::move(status));

    Editor::State& editor = Editor::Global();
    editor.LoadDocument(std::move(document));
    editor.MutView().px_per_frame = 1.0;
    editor.MutView().snap = false;
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
    return Editor::ClipById(Editor::Global().Document(), id);
}

float AlphaAt(int frame) {
    const Doc::Document& document = Editor::Global().Document();
    const std::vector<int> choices;
    const Preset::Eval::ResolveInput input{
        .document = &document, .choices = &choices, .tweens = true};
    const Preset::Eval::FrameState state = Preset::Eval::ResolveFrame(input, frame);
    for (const Preset::Eval::ModelSlot& slot : state.models) {
        if (slot.name == "core") return slot.alpha;
    }
    return -1.0F;
}

void FocusEditor(ImGuiTestContext* ctx) {
    ctx->SetRef("##main");
    GuiTest::FocusChild(ctx, "main_view/##timeline_editor");
}

}

TEST_CASE("K adds a tween key at the playhead on the selected clip", "[gui][timeline][tween]") {
    GuiTest::Harness harness;
    OpenDocument(MakeDocument(), 150, {});

    ImGuiTest* test = harness.NewTest("tl_key_add");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_clip_core_fade");
        ctx->Yield(2);
        ctx->KeyPress(ImGuiKey_K);
        ctx->Yield(3);
    };
    harness.Run(test);

    REQUIRE(Find("core_fade") != nullptr);
    REQUIRE(Find("core_fade")->keys.size() == 3);
    CHECK(Find("core_fade")->keys[1].at == 50);
    CHECK(AlphaAt(150) == Catch::Approx(0.5));
    Editor::Global().Close();
}

TEST_CASE("the timeline shows a key diamond that can be dragged in time",
          "[gui][timeline][tween]") {
    GuiTest::Harness harness;
    OpenDocument(MakeDocument(), 150, {});

    ImGuiTest* test = harness.NewTest("tl_key_drag");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        const ImGuiTestItemInfo info = ctx->ItemInfo("###tl_key_core_fade_1");
        IM_CHECK_NE(info.ID, 0U);
        const ImVec2 middle((info.RectFull.Min.x + info.RectFull.Max.x) * 0.5F,
                            (info.RectFull.Min.y + info.RectFull.Max.y) * 0.5F);
        ctx->MouseMoveToPos(middle);
        ctx->MouseDown(0);
        ctx->MouseMoveToPos(ImVec2(middle.x - 30.0F, middle.y));
        ctx->Yield(3);
        ctx->MouseUp(0);
        ctx->Yield(2);
    };
    harness.Run(test);

    REQUIRE(Find("core_fade") != nullptr);
    CHECK(Find("core_fade")->keys[1].at == 70);
    Editor::Global().Close();
}

TEST_CASE("dragging a key in the curve editor changes the value the evaluator resolves",
          "[gui][timeline][curve]") {
    GuiTest::Harness harness;
    OpenDocument(MakeDocument(), 150, {});
    const float before = AlphaAt(150);

    ImGuiTest* test = harness.NewTest("tl_curve_drag");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_clip_core_fade");
        ctx->Yield(2);
        ctx->KeyPress(ImGuiKey_C);
        ctx->Yield(3);
        IM_CHECK(ctx->ItemExists("###tl_curve_close"));

        const ImGuiTestItemInfo info = ctx->ItemInfo("###tl_curve_key_0");
        IM_CHECK_NE(info.ID, 0U);
        const ImVec2 middle((info.RectFull.Min.x + info.RectFull.Max.x) * 0.5F,
                            (info.RectFull.Min.y + info.RectFull.Max.y) * 0.5F);
        ctx->MouseMoveToPos(middle);
        ctx->MouseDown(0);
        ctx->MouseMoveToPos(ImVec2(middle.x, middle.y - 40.0F));
        ctx->Yield(3);
        ctx->MouseUp(0);
        ctx->Yield(2);
        ctx->ItemClick("###tl_curve_close");
        ctx->Yield(2);
    };
    harness.Run(test);

    REQUIRE(Find("core_fade") != nullptr);
    const Doc::KeyValue* value = Editor::KeyValueOf(Find("core_fade")->keys[0], "alpha");
    REQUIRE(value != nullptr);
    CHECK(std::get<double>(value->value) > 0.0);
    CHECK(AlphaAt(150) > before);
    Editor::Global().Close();
}

TEST_CASE("the Tween tab adds a key at the playhead and lists the keys", "[gui][timeline][tween]") {
    GuiTest::Harness harness;
    OpenDocument(MakeDocument(), 130, {});

    ImGuiTest* test = harness.NewTest("tl_tween_tab");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemDoubleClick("###tl_clip_core_fade");
        ctx->Yield(6);
        ctx->SetRef("Clip properties");
        GuiTest::FocusChild(ctx, "//Clip properties/###tl_clip_body");
        ctx->ItemClick("###tl_clip_tabs/###tl_tab_tween");
        ctx->Yield(2);
        ctx->ItemClick("###tl_clip_tabs/###tl_tab_tween/###tl_tween_add_key");
        ctx->Yield(2);
        ctx->ItemClick("//Clip properties/###tl_clip_done");
        ctx->Yield(2);
    };
    harness.Run(test);

    REQUIRE(Find("core_fade") != nullptr);
    REQUIRE(Find("core_fade")->keys.size() == 3);
    CHECK(Find("core_fade")->keys[1].at == 30);
    Editor::Global().Close();
}

TEST_CASE("the options track posts SetOption and shows the selected choice",
          "[gui][timeline][options]") {
    GuiTest::Harness harness;
    OpenDocument(MakeOptionDocument(), 0, {2});

    ImGuiTest* test = harness.NewTest("tl_options_track");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        IM_CHECK(ctx->ItemExists("###tl_opt_head_mode"));
        const std::string text = GuiTest::CaptureFrameText(ctx);
        IM_CHECK(text.find("selected FREE") != std::string::npos);
        IM_CHECK(text.find("100 f at step 4, kick 15") != std::string::npos);
        ctx->ItemClick("###tl_opt_choice_mode_1");
        ctx->Yield(2);
    };
    harness.Run(test);

    int option = -1;
    int choice = -1;
    for (const PresetCmd::Any& command : DrainPresetCommands()) {
        const auto* set = std::get_if<PresetCmd::SetOption>(&command);
        if (set == nullptr) continue;
        option = set->option;
        choice = set->choice;
    }
    CHECK(option == 0);
    CHECK(choice == 1);
    Editor::Global().Close();
}

TEST_CASE("the option properties modal edits the transition frames", "[gui][timeline][options]") {
    GuiTest::Harness harness;
    OpenDocument(MakeOptionDocument(), 0, {0});

    ImGuiTest* test = harness.NewTest("tl_option_modal");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_opt_edit_mode");
        ctx->Yield(3);
        ctx->SetRef("Option properties");
        ctx->ItemInputValue("###tl_option_frames", 40);
        ctx->Yield(2);
        ctx->ItemClick("###tl_option_done");
        ctx->Yield(2);
    };
    harness.Run(test);

    REQUIRE_FALSE(Editor::Global().Document().options.empty());
    CHECK(Editor::Global().Document().options.front().transition.frames == 40);
    Editor::Global().Close();
}

TEST_CASE("the option properties modal moves a choice down and carries the default",
          "[gui][timeline][options]") {
    GuiTest::Harness harness;
    OpenDocument(MakeOptionDocument(), 0, {0});

    ImGuiTest* test = harness.NewTest("tl_option_reorder");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_opt_edit_mode");
        ctx->Yield(3);
        ctx->SetRef("Option properties");
        ctx->ItemClick("###tl_option_choice_down_0");
        ctx->Yield(2);
        ctx->ItemClick("###tl_option_done");
        ctx->Yield(2);
    };
    harness.Run(test);

    REQUIRE_FALSE(Editor::Global().Document().options.empty());
    const Doc::OptionSpec& option = Editor::Global().Document().options.front();
    REQUIRE(option.choices.size() == 3);
    CHECK(option.choices[0].label == "EXPERT");
    CHECK(option.choices[1].label == "BEGINNER");
    CHECK(option.default_choice == 1);
    Editor::Global().Close();
}

TEST_CASE("add transition to next clip inserts a tween below the draw track",
          "[gui][timeline][transition]") {
    GuiTest::Harness harness;
    OpenDocument(MakeDocument(), 0, {});
    const std::size_t tracks = Editor::Global().Document().tracks.size();

    ImGuiTest* test = harness.NewTest("tl_add_transition");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_clip_core_a", ImGuiMouseButton_Right);
        ctx->Yield(3);
        ctx->ItemClick("//$FOCUSED/###tl_menu_transition_next");
        ctx->Yield(3);
    };
    harness.Run(test);

    const Doc::Document& document = Editor::Global().Document();
    CHECK(document.tracks.size() == tracks + 1);
    const Editor::ClipRef ref = Editor::FindClip(document, "core_a");
    REQUIRE(ref.Valid());
    const Doc::Track& inserted = document.tracks[(std::size_t)ref.track + 1];
    REQUIRE(inserted.clips.size() == 1);
    CHECK(Doc::TypeOf(inserted.clips.front().command) == Doc::CommandType::ModelTween);
    CHECK(inserted.clips.front().start == 170);
    CHECK(inserted.clips.front().end.value_or(0) == 230);
    Editor::Global().Close();
}
