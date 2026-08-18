#include "gui_test_harness.h"

#include "editor/export_range.h"
#include "editor/preset_editor_state.h"
#include "editor/timeline_edits.h"
#include "editor/timeline_view.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "state/preset_commands.h"
#include "state/telemetry.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

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

Doc::Clip Draw(const std::string& id, int start, std::optional<int> end, const std::string& model) {
    Doc::Clip clip;
    clip.id = id;
    clip.start = start;
    clip.end = end;
    clip.command = Doc::ModelDraw{.asset = "scene", .model = model};
    return clip;
}

Doc::Document MakeDocument() {
    Doc::Document document;
    document.id = "gui-editor-test";
    document.name = "GUI editor test";
    document.build = "iidx11";
    document.length = kLength;
    document.assets.push_back(
        Doc::Asset{.id = "scene", .kind = Doc::AssetKind::Scene3d, .dir = "data/model"});
    document.markers.push_back(Doc::Marker{.frame = 0, .label = "start"});

    Doc::Track core;
    core.id = "core";
    core.name = "core";
    core.kind = Doc::TrackKind::Model;
    core.target = "core";
    core.clips.push_back(Draw("core_a", 0, 100, "core"));
    core.clips.push_back(Draw("core_b", 200, 300, "core"));
    document.tracks.push_back(std::move(core));

    Doc::Track shield;
    shield.id = "shield";
    shield.name = "shield";
    shield.kind = Doc::TrackKind::Model;
    shield.target = "shield";
    shield.clips.push_back(Draw("shield_a", 50, 400, "shield"));
    document.tracks.push_back(std::move(shield));
    return document;
}

Doc::Document MakeTallDocument() {
    Doc::Document document = MakeDocument();
    document.tracks.clear();
    for (int i = 0; i < 14; i++) {
        const std::string name = "t" + std::to_string(i);
        Doc::Track track;
        track.id = name;
        track.name = name;
        track.kind = Doc::TrackKind::Model;
        track.target = name;
        track.clips.push_back(Draw("clip_" + name, 40 * i, 40 * (i + 1), name));
        document.tracks.push_back(std::move(track));
    }
    return document;
}

Doc::Clip Modifier(const std::string& id, Doc::Command command) {
    Doc::Clip clip;
    clip.id = id;
    clip.start = 0;
    clip.end = 200;
    clip.command = std::move(command);
    return clip;
}

Doc::Track OneTrack(const std::string& id, Doc::TrackKind kind, std::vector<Doc::Clip> clips) {
    Doc::Track track;
    track.id = id;
    track.name = id;
    track.kind = kind;
    track.target = id;
    track.clips = std::move(clips);
    return track;
}

Doc::Document MakeLaneDocument() {
    Doc::Document document = MakeDocument();
    document.tracks.clear();
    document.tracks.push_back(OneTrack(
        "core", Doc::TrackKind::Model,
        {Draw("clip_core", 0, 200, "core"), Modifier("clip_core_motion", Doc::ModelMotionCmd{})}));
    document.tracks.push_back(
        OneTrack("shield", Doc::TrackKind::Model, {Draw("clip_shield", 0, 200, "shield")}));
    document.tracks.push_back(OneTrack("camera_tween", Doc::TrackKind::Camera,
                                       {Modifier("clip_camera_tween", Doc::CameraTween{})}));
    document.tracks.push_back(
        OneTrack("fx", Doc::TrackKind::Fx, {Modifier("clip_fx", Doc::EmitterCmd{})}));
    return document;
}

void OpenDocument(Doc::Document document, int frame) {
    GuiTest::EnterReadyView("scene3d", "iidx11");
    App::PresetStatus status;
    status.id = "gui-editor-test";
    status.frame = frame;
    status.length = kLength;
    status.fps = 60;
    status.playing = false;
    status.loop = true;
    App::Global().SetPresetStatus(std::move(status));

    Editor::State& editor = Editor::Global();
    editor.LoadDocument(std::move(document));
    editor.MutView().px_per_frame = 1.0;
    editor.MutView().snap = false;
    while (App::Global().TakeCommand().has_value()) {
    }
}

void OpenEditor(int frame) {
    OpenDocument(MakeDocument(), frame);
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

int LastSeek(const std::vector<PresetCmd::Any>& commands) {
    int frame = -1;
    for (const PresetCmd::Any& command : commands) {
        if (const auto* seek = std::get_if<PresetCmd::Seek>(&command)) frame = seek->frame;
    }
    return frame;
}

bool HasReplace(const std::vector<PresetCmd::Any>& commands) {
    return std::ranges::any_of(commands, [](const PresetCmd::Any& command) {
        return std::holds_alternative<PresetCmd::ReplaceDocument>(command);
    });
}

const Doc::Clip* Find(const std::string& id) {
    const Doc::Document& document = Editor::Global().Document();
    const Editor::ClipRef ref = Editor::FindClip(document, id);
    if (!ref.Valid()) return nullptr;
    return &document.tracks[(std::size_t)ref.track].clips[(std::size_t)ref.clip];
}

void FocusEditor(ImGuiTestContext* ctx) {
    ctx->SetRef("##main");
    GuiTest::FocusChild(ctx, "main_view/##timeline_editor");
}

}

TEST_CASE("the timeline editor replaces the fixed dock while a document is loaded",
          "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(0);

    ImGuiTest* test = harness.NewTest("tl_editor_present");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        IM_CHECK(ctx->ItemExists("###tl_ruler"));
        IM_CHECK(ctx->ItemExists("###tl_play"));
        IM_CHECK(ctx->ItemExists("###tl_clip_core_a"));
        IM_CHECK(ctx->ItemExists("###tl_head_core"));
        ctx->SetRef("##main");
        IM_CHECK(ctx->ItemExists("main_view/##timeline_dock") == false);
    };
    harness.Run(test);
    Editor::Global().Close();
}

TEST_CASE("clicking the ruler posts a Seek to the render thread", "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(0);

    ImGuiTest* test = harness.NewTest("tl_ruler_seek");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        const ImGuiTestItemInfo info = ctx->ItemInfo("###tl_ruler");
        IM_CHECK_NE(info.ID, 0U);
        ctx->MouseMoveToPos(ImVec2(info.RectFull.Min.x + 120.0F,
                                   (info.RectFull.Min.y + info.RectFull.Max.y) * 0.5F));
        ctx->MouseDown(0);
        ctx->Yield(2);
        ctx->MouseUp(0);
        ctx->Yield(2);
    };
    harness.Run(test);

    const std::vector<PresetCmd::Any> commands = DrainPresetCommands();
    CHECK(LastSeek(commands) == 120);
    Editor::Global().Close();
}

TEST_CASE("the transport buttons post seek and pause commands", "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(10);

    ImGuiTest* test = harness.NewTest("tl_transport");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_step_fwd");
        ctx->ItemClick("###tl_jump_end");
        ctx->ItemClick("###tl_play");
    };
    harness.Run(test);

    const std::vector<PresetCmd::Any> commands = DrainPresetCommands();
    CHECK(LastSeek(commands) == kLength - 1);
    bool resumed = false;
    for (const PresetCmd::Any& command : commands) {
        const auto* paused = std::get_if<PresetCmd::SetPaused>(&command);
        if (paused != nullptr) resumed = !paused->paused;
    }
    CHECK(resumed);
    Editor::Global().Close();
}

TEST_CASE("the loop toggle posts SetLoop", "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(0);

    ImGuiTest* test = harness.NewTest("tl_loop_toggle");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_loop");
    };
    harness.Run(test);

    bool off = false;
    for (const PresetCmd::Any& command : DrainPresetCommands()) {
        const auto* loop = std::get_if<PresetCmd::SetLoop>(&command);
        if (loop != nullptr) off = !loop->loop;
    }
    CHECK(off);
    Editor::Global().Close();
}

TEST_CASE("dragging a clip moves its start by the frame delta and publishes the document",
          "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(0);

    ImGuiTest* test = harness.NewTest("tl_clip_drag");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        const ImGuiTestItemInfo info = ctx->ItemInfo("###tl_clip_core_b");
        IM_CHECK_NE(info.ID, 0U);
        const ImVec2 middle((info.RectFull.Min.x + info.RectFull.Max.x) * 0.5F,
                            (info.RectFull.Min.y + info.RectFull.Max.y) * 0.5F);
        ctx->MouseMoveToPos(middle);
        ctx->MouseDown(0);
        ctx->MouseMoveToPos(ImVec2(middle.x + 40.0F, middle.y));
        ctx->Yield(3);
        ctx->MouseUp(0);
        ctx->Yield(2);
    };
    harness.Run(test);

    REQUIRE(Find("core_b") != nullptr);
    CHECK(Find("core_b")->start == 240);
    CHECK(Find("core_b")->end.value_or(0) == 340);
    CHECK(Editor::Global().UndoDepth() == 1);
    CHECK(HasReplace(DrainPresetCommands()));
    Editor::Global().Close();
}

TEST_CASE("a double click on a clip selects it and opens the properties modal",
          "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(0);

    ImGuiTest* test = harness.NewTest("tl_clip_double_click");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemDoubleClick("###tl_clip_core_b");
        ctx->Yield(3);
        IM_CHECK(ctx->WindowInfo("//Clip properties").Window != nullptr);
        ctx->SetRef("Clip properties");
        ctx->ItemClick("###tl_clip_cancel");
        ctx->Yield(2);
    };
    harness.Run(test);

    CHECK(Editor::Global().IsSelected("core_b"));
    Editor::Global().Close();
}

TEST_CASE("the track mute toggle mutes the track in the document", "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(0);

    ImGuiTest* test = harness.NewTest("tl_track_mute");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_mute_core");
        ctx->Yield(2);
    };
    harness.Run(test);

    REQUIRE(Editor::Global().Document().tracks.size() == 2);
    CHECK(Editor::Global().Document().tracks[0].muted);
    CHECK_FALSE(Editor::Global().Document().tracks[1].muted);
    CHECK(HasReplace(DrainPresetCommands()));
    Editor::Global().Close();
}

TEST_CASE("undo puts a dragged clip back where it was", "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(0);

    ImGuiTest* test = harness.NewTest("tl_undo");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_mute_core");
        ctx->Yield(2);
        ctx->KeyDown(ImGuiKey_LeftCtrl);
        ctx->KeyPress(ImGuiKey_Z);
        ctx->KeyUp(ImGuiKey_LeftCtrl);
        ctx->Yield(3);
    };
    harness.Run(test);

    CHECK_FALSE(Editor::Global().Document().tracks[0].muted);
    CHECK(Editor::Global().UndoDepth() == 0);
    CHECK(Editor::Global().RedoDepth() == 1);
    Editor::Global().Close();
}

TEST_CASE("deleting the selection removes the clip", "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(0);

    ImGuiTest* test = harness.NewTest("tl_delete");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_clip_core_b");
        ctx->Yield(2);
        ctx->KeyPress(ImGuiKey_Delete);
        ctx->Yield(3);
    };
    harness.Run(test);

    CHECK(Find("core_b") == nullptr);
    CHECK(Find("core_a") != nullptr);
    Editor::Global().Close();
}

TEST_CASE("the zoom slider and fit button drive the view", "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(0);

    ImGuiTest* test = harness.NewTest("tl_zoom");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemInputValue("###tl_zoom", 2.0F);
        ctx->Yield(2);
    };
    harness.Run(test);
    CHECK(Editor::Global().GetView().px_per_frame > 1.5);

    ImGuiTest* fit = harness.NewTest("tl_fit");
    fit->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->ItemClick("###tl_fit");
        ctx->Yield(2);
    };
    harness.Run(fit);
    CHECK(Editor::Global().GetView().px_per_frame < 2.0);
    CHECK(Editor::Global().GetView().px_per_frame > Editor::kZoomMin);
    CHECK(Editor::Global().GetView().scroll == 0.0);
    Editor::Global().Close();
}

TEST_CASE("every header row is exactly as tall as its lane and holds its own clips",
          "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenDocument(MakeLaneDocument(), 0);

    ImGuiTest* test = harness.NewTest("tl_lane_rows");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        const char* const tracks[] = {"core", "shield", "camera_tween", "fx"};
        const char* const clips[] = {"clip_core", "clip_shield", "clip_camera_tween", "clip_fx"};
        float previous_bottom = 0.0F;
        for (int i = 0; i < 4; i++) {
            const ImGuiTestItemInfo head =
                ctx->ItemInfo((std::string("###tl_head_") + tracks[i]).c_str());
            const ImGuiTestItemInfo lane =
                ctx->ItemInfo((std::string("###tl_lane_") + tracks[i]).c_str());
            IM_CHECK_NE(head.ID, 0U);
            IM_CHECK_NE(lane.ID, 0U);
            IM_CHECK_EQ(head.RectFull.GetHeight(), lane.RectFull.GetHeight());
            IM_CHECK_EQ(head.RectFull.Min.y, lane.RectFull.Min.y);
            if (i > 0) IM_CHECK_EQ(lane.RectFull.Min.y, previous_bottom);
            previous_bottom = lane.RectFull.Max.y;

            const ImGuiTestItemInfo clip =
                ctx->ItemInfo((std::string("###tl_clip_") + clips[i]).c_str());
            IM_CHECK_NE(clip.ID, 0U);
            IM_CHECK_GE(clip.RectFull.Min.y, lane.RectFull.Min.y);
            IM_CHECK_LE(clip.RectFull.Max.y, lane.RectFull.Max.y);
        }

        const ImGuiTestItemInfo plain = ctx->ItemInfo("###tl_lane_shield");
        const ImGuiTestItemInfo tween = ctx->ItemInfo("###tl_lane_camera_tween");
        const ImGuiTestItemInfo mixed = ctx->ItemInfo("###tl_lane_core");
        IM_CHECK_EQ(tween.RectFull.GetHeight(), plain.RectFull.GetHeight());
        IM_CHECK_EQ(ctx->ItemInfo("###tl_lane_fx").RectFull.GetHeight(),
                    plain.RectFull.GetHeight());
        IM_CHECK_GT(mixed.RectFull.GetHeight(), plain.RectFull.GetHeight());

        const ImGuiTestItemInfo tween_clip = ctx->ItemInfo("###tl_clip_clip_camera_tween");
        const ImGuiTestItemInfo plain_clip = ctx->ItemInfo("###tl_clip_clip_shield");
        IM_CHECK_EQ(tween_clip.RectFull.GetHeight(), plain_clip.RectFull.GetHeight());
        IM_CHECK_EQ(tween_clip.RectFull.Min.y - tween.RectFull.Min.y,
                    plain_clip.RectFull.Min.y - plain.RectFull.Min.y);
    };
    harness.Run(test);
    Editor::Global().Close();
}

TEST_CASE("scrolling the lanes leaves the transport, ruler and scroll bar fixed",
          "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenDocument(MakeTallDocument(), 0);

    ImGuiTest* test = harness.NewTest("tl_lane_scroll");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        const ImRect play = ctx->ItemInfo("###tl_play").RectFull;
        const ImRect ruler = ctx->ItemInfo("###tl_ruler").RectFull;
        const ImRect bar = ctx->ItemInfo("###tl_scroll").RectFull;
        const ImGuiWindow* editor = ctx->WindowInfo("//$FOCUSED").Window;
        IM_CHECK(editor != nullptr);

        ctx->MouseMove("###tl_lane_t0");
        ctx->KeyDown(ImGuiKey_LeftShift);
        ctx->MouseWheelY(-20.0F);
        ctx->Yield(3);

        const ImGuiTestItemInfo play_after = ctx->ItemInfo("###tl_play");
        const ImGuiTestItemInfo ruler_after = ctx->ItemInfo("###tl_ruler");
        const ImGuiTestItemInfo bar_after = ctx->ItemInfo("###tl_scroll");
        IM_CHECK_EQ(play_after.RectFull.Min.y, play.Min.y);
        IM_CHECK_EQ(play_after.RectFull.Max.y, play.Max.y);
        IM_CHECK_EQ(play_after.RectClipped.GetHeight(), play_after.RectFull.GetHeight());
        IM_CHECK_EQ(ruler_after.RectFull.Min.y, ruler.Min.y);
        IM_CHECK_EQ(ruler_after.RectFull.Max.y, ruler.Max.y);
        IM_CHECK_EQ(bar_after.RectFull.Min.y, bar.Min.y);
        IM_CHECK_LE(bar_after.RectFull.Max.y, editor->Pos.y + editor->Size.y);

        ctx->MouseWheelY(6.5F);
        ctx->Yield(3);
        for (int i = 0; i < 14; i++) {
            const std::string id = "###tl_clip_clip_t" + std::to_string(i);
            const ImGuiTestItemInfo part = ctx->ItemInfo(id.c_str(), ImGuiTestOpFlags_NoError);
            if (part.ID == 0U || part.RectClipped.GetHeight() <= 0.0F) continue;
            IM_CHECK_LE(part.RectClipped.Max.y, bar_after.RectFull.Min.y);
            IM_CHECK_GE(part.RectClipped.Min.y, ruler_after.RectFull.Max.y);
        }
        ctx->MouseWheelY(-20.0F);
        ctx->KeyUp(ImGuiKey_LeftShift);
        ctx->Yield(3);

        const ImGuiTestItemInfo last = ctx->ItemInfo("###tl_clip_clip_t13");
        const ImGuiTestItemInfo head = ctx->ItemInfo("###tl_head_t13");
        IM_CHECK_NE(last.ID, 0U);
        IM_CHECK_GT(last.RectClipped.GetHeight(), 0.0F);
        IM_CHECK_EQ(last.RectClipped.GetHeight(), last.RectFull.GetHeight());
        IM_CHECK_LE(last.RectFull.Max.y, bar_after.RectFull.Min.y);
        IM_CHECK_LE(head.RectFull.Max.y, bar_after.RectFull.Min.y);
        IM_CHECK_GE(last.RectFull.Min.y, ruler_after.RectFull.Max.y);
    };
    harness.Run(test);
    Editor::Global().Close();
}

TEST_CASE("the M S L toggles fit their letters inside the header column",
          "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(0);

    ImGuiTest* test = harness.NewTest("tl_header_toggles");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        const ImGuiWindow* editor = ctx->WindowInfo("//$FOCUSED").Window;
        IM_CHECK(editor != nullptr);
        const ImGuiTestItemInfo split = ctx->ItemInfo("###tl_header_split");
        IM_CHECK_NE(split.ID, 0U);
        const float header_right = (split.RectFull.Min.x + split.RectFull.Max.x) * 0.5F;
        const ImVec2 pad = ImGui::GetStyle().FramePadding;

        for (const char* id : {"###tl_mute_core", "###tl_solo_core", "###tl_lock_core"}) {
            const ImGuiTestItemInfo info = ctx->ItemInfo(id);
            IM_CHECK_NE(info.ID, 0U);
            const ImVec2 letter = ImGui::CalcTextSize("M");
            IM_CHECK_GE(info.RectFull.GetWidth(), letter.x + (2.0F * pad.x));
            IM_CHECK_GE(info.RectFull.GetHeight(), letter.y + (2.0F * pad.y));
            IM_CHECK_EQ(info.RectClipped.GetWidth(), info.RectFull.GetWidth());
            IM_CHECK_EQ(info.RectClipped.GetHeight(), info.RectFull.GetHeight());
            IM_CHECK_GE(info.RectFull.Min.x, editor->Pos.x);
            IM_CHECK_LE(info.RectFull.Max.x, header_right);
        }
    };
    harness.Run(test);

    Editor::Global().MutView().header_w = Editor::kHeaderWidthMin;
    harness.Run(test);
    CHECK(Editor::Global().GetView().header_w == Editor::kHeaderWidthMin);
    Editor::Global().Close();
}

TEST_CASE("the track header column drags between its bounds", "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(0);
    const float before = Editor::Global().GetView().header_w;

    ImGuiTest* wider = harness.NewTest("tl_header_split_wider");
    wider->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        const ImGuiTestItemInfo info = ctx->ItemInfo("###tl_header_split");
        IM_CHECK_NE(info.ID, 0U);
        const ImVec2 grip((info.RectFull.Min.x + info.RectFull.Max.x) * 0.5F,
                          info.RectFull.Min.y + 20.0F);
        ctx->MouseMoveToPos(grip);
        ctx->MouseDown(0);
        ctx->MouseMoveToPos(ImVec2(grip.x + 60.0F, grip.y));
        ctx->Yield(3);
        ctx->MouseUp(0);
        ctx->Yield(2);
    };
    harness.Run(wider);
    CHECK(Editor::Global().GetView().header_w > before);
    CHECK(Editor::Global().GetView().header_w <= Editor::kHeaderWidthMax);

    ImGuiTest* clamped = harness.NewTest("tl_header_split_clamped");
    clamped->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        const ImGuiTestItemInfo info = ctx->ItemInfo("###tl_header_split");
        const ImVec2 grip((info.RectFull.Min.x + info.RectFull.Max.x) * 0.5F,
                          info.RectFull.Min.y + 20.0F);
        ctx->MouseMoveToPos(grip);
        ctx->MouseDown(0);
        ctx->MouseMoveToPos(ImVec2(grip.x - 600.0F, grip.y));
        ctx->Yield(3);
        ctx->MouseUp(0);
        ctx->Yield(2);
    };
    harness.Run(clamped);
    CHECK(Editor::Global().GetView().header_w == Editor::kHeaderWidthMin);
    Editor::Global().Close();
}

TEST_CASE("the editor spans the window width above the status strip", "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(0);

    ImGuiTest* test = harness.NewTest("tl_bottom_dock");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        const ImGuiWindow* row = ctx->WindowInfo("main_view").Window;
        const ImGuiWindow* editor = ctx->WindowInfo("main_view/##timeline_editor").Window;
        const ImGuiWindow* strip = ctx->WindowInfo("status_strip").Window;
        IM_CHECK(row != nullptr);
        IM_CHECK(editor != nullptr);
        IM_CHECK(strip != nullptr);

        IM_CHECK_EQ(editor->Pos.x, row->Pos.x);
        IM_CHECK_EQ(editor->Pos.x + editor->Size.x, row->Pos.x + row->Size.x);
        IM_CHECK_GT(editor->Size.x, row->Size.x - 1.0F);
        IM_CHECK_LE(editor->Pos.y + editor->Size.y, strip->Pos.y + 1.0F);
        IM_CHECK_GT(editor->Pos.y + editor->Size.y, strip->Pos.y - 24.0F);

        const ImGuiWindow* left = ctx->WindowInfo("main_view/pane_left").Window;
        IM_CHECK(left != nullptr);
        IM_CHECK_LE(left->Pos.y + left->Size.y, editor->Pos.y + 1.0F);
    };
    harness.Run(test);
    Editor::Global().Close();
}

TEST_CASE("the editor splitter gives the editor most of the window", "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(0);
    const float before = Editor::Global().GetView().height;
    CHECK(before == Editor::kEditorHeightDefault);

    ImGuiTest* test = harness.NewTest("tl_splitter_drag");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view");
        const ImGuiTestItemInfo info = ctx->ItemInfo("##split_timeline");
        IM_CHECK_NE(info.ID, 0U);
        const ImVec2 grip((info.RectFull.Min.x + info.RectFull.Max.x) * 0.5F,
                          (info.RectFull.Min.y + info.RectFull.Max.y) * 0.5F);
        ctx->MouseMoveToPos(grip);
        ctx->MouseDown(0);
        ctx->MouseMoveToPos(ImVec2(grip.x, grip.y - 260.0F));
        ctx->Yield(3);
        ctx->MouseUp(0);
        ctx->Yield(2);
    };
    harness.Run(test);

    CHECK(Editor::Global().GetView().height > before + 200.0F);
    Editor::Global().Close();
}

TEST_CASE("the whole transport row fits the default window width", "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(0);
    GuiTest::SetDisplaySize(1344.0F, 800.0F);

    ImGuiTest* test = harness.NewTest("tl_transport_fits");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        for (const char* id : {"###tl_zoom", "###tl_fit", "###tl_doc_badge", "###tl_next_edge"}) {
            const ImGuiTestItemInfo info = ctx->ItemInfo(id);
            IM_CHECK_NE(info.ID, 0U);
            IM_CHECK_GT(info.RectClipped.GetWidth(), 0.0F);
            IM_CHECK_EQ(info.RectClipped.GetWidth(), info.RectFull.GetWidth());
        }
    };
    harness.Run(test);
    GuiTest::SetDisplaySize(1600.0F, 900.0F);
    Editor::Global().Close();
}

TEST_CASE("the transport counter counts the frames the document has", "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(120);

    ImGuiTest* test = harness.NewTest("tl_counter");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        const std::string text = GuiTest::CaptureFrameText(ctx);
        IM_CHECK(text.find("120 / 600") != std::string::npos);
    };
    harness.Run(test);
    Editor::Global().Close();
}

TEST_CASE("shift plus arrow steps one hundred document frames", "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(10);

    ImGuiTest* test = harness.NewTest("tl_shift_step");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        ctx->MouseMove("###tl_ruler");
        ctx->KeyPress(ImGuiMod_Shift | ImGuiKey_RightArrow);
        ctx->Yield(2);
    };
    harness.Run(test);

    CHECK(LastSeek(DrainPresetCommands()) == 110);
    Editor::Global().Close();
}

TEST_CASE("the rubber band selects only the clips its rectangle covers",
          "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(0);

    ImGuiTest* test = harness.NewTest("tl_rubber_band");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        const ImGuiTestItemInfo core = ctx->ItemInfo("###tl_clip_core_b");
        const ImGuiTestItemInfo lane = ctx->ItemInfo("###tl_lane_core");
        IM_CHECK_NE(core.ID, 0U);
        IM_CHECK_NE(lane.ID, 0U);
        const float y = (lane.RectFull.Min.y + lane.RectFull.Max.y) * 0.5F;
        ctx->MouseMoveToPos(ImVec2(core.RectFull.Min.x - 20.0F, y));
        ctx->MouseDown(0);
        ctx->MouseMoveToPos(ImVec2(core.RectFull.Max.x + 20.0F, y));
        ctx->Yield(2);
        ctx->MouseUp(0);
        ctx->Yield(2);
    };
    harness.Run(test);

    CHECK(Editor::Global().IsSelected("core_b"));
    CHECK_FALSE(Editor::Global().IsSelected("shield_a"));
    CHECK(Editor::Global().Selection().size() == 1);
    Editor::Global().Close();
}

TEST_CASE("the fixed 76 px dock still serves the AFP backends", "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    Editor::Global().Close();
    App::Global().SetPresetStatus({});
    GuiTest::EnterReadyView("afp_modern", "iidx17");
    GuiTest::LoadScene("test.ifs", 10, 100);

    ImGuiTest* test = harness.NewTest("tl_dock_fallback");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->SetRef("##main");
        GuiTest::FocusChild(ctx, "main_view/##timeline_dock");
        IM_CHECK(ctx->ItemExists("##tl_track"));
        IM_CHECK(ctx->ItemExists("###tl_ruler") == false);
    };
    harness.Run(test);
}

TEST_CASE("shift dragging the ruler sets the export range", "[gui][timeline][editor]") {
    GuiTest::Harness harness;
    OpenEditor(0);

    ImGuiTest* test = harness.NewTest("tl_export_range");
    test->TestFunc = [](ImGuiTestContext* ctx) {
        FocusEditor(ctx);
        const ImGuiTestItemInfo info = ctx->ItemInfo("###tl_ruler");
        IM_CHECK_NE(info.ID, 0U);
        const float y = (info.RectFull.Min.y + info.RectFull.Max.y) * 0.5F;
        ctx->MouseMoveToPos(ImVec2(info.RectFull.Min.x + 100.0F, y));
        ctx->KeyDown(ImGuiKey_LeftShift);
        ctx->MouseDown(0);
        ctx->MouseMoveToPos(ImVec2(info.RectFull.Min.x + 260.0F, y));
        ctx->Yield(2);
        ctx->MouseUp(0);
        ctx->KeyUp(ImGuiKey_LeftShift);
        ctx->Yield(2);
    };
    harness.Run(test);

    const Editor::ExportRange range = Editor::Global().GetView().export_range;
    CHECK(range.active);
    CHECK(range.start == 100);
    CHECK(range.end == 261);
    Editor::Global().Close();
}
