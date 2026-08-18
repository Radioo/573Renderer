#include <catch2/catch_test_macros.hpp>

#include "editor/clip_summary.h"
#include "editor/preset_editor_state.h"
#include "editor/timeline_drag.h"
#include "editor/timeline_edits.h"
#include "editor/timeline_lanes.h"
#include "editor/timeline_view.h"
#include "formats/gcanim.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/eval_resolve.h"
#include "preset/eval/frame_state.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace Doc = Preset::Doc;

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
    document.id = "editor-test";
    document.name = "Editor test";
    document.build = "iidx11";
    document.length = 600;
    document.assets.push_back(
        Doc::Asset{.id = "scene", .kind = Doc::AssetKind::Scene3d, .dir = "data/model"});

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

const Doc::Clip* Find(const Doc::Document& document, const std::string& id) {
    const Editor::ClipRef ref = Editor::FindClip(document, id);
    if (!ref.Valid()) return nullptr;
    return &document.tracks[(std::size_t)ref.track].clips[(std::size_t)ref.clip];
}

Editor::DragStart GrabBody(const Doc::Document& document, const std::string& id, int grab_frame) {
    const Doc::Clip* clip = Find(document, id);
    REQUIRE(clip != nullptr);
    const Editor::ClipRef ref = Editor::FindClip(document, id);
    Editor::DragStart drag;
    drag.mode = Editor::DragMode::Move;
    drag.clip_id = id;
    drag.track_id = document.tracks[(std::size_t)ref.track].id;
    drag.start = clip->start;
    drag.end = clip->end;
    drag.grab_frame = grab_frame;
    return drag;
}

Editor::DragInput At(int cursor, const std::string& track) {
    Editor::DragInput input;
    input.cursor_frame = cursor;
    input.track_id = track;
    input.px_per_frame = 1.0;
    input.playhead = -1;
    input.snap = false;
    return input;
}

std::string Summary(const Doc::Command& command) {
    return Editor::CommandSummary(command, {});
}

bool ModelVisible(const Doc::Document& document, int frame, const std::string& model) {
    const std::vector<int> choices;
    const Preset::Eval::ResolveInput input{
        .document = &document, .choices = &choices, .tweens = true};
    const Preset::Eval::FrameState state = Preset::Eval::ResolveFrame(input, frame);
    const auto it = std::ranges::find_if(
        state.models, [&model](const Preset::Eval::ModelSlot& slot) { return slot.name == model; });
    return it != state.models.end() && it->visible;
}

}

TEST_CASE("the ruler picks the finest spacing that keeps labels 56 px apart", "[editor][ruler]") {
    CHECK(Editor::RulerStep(64.0) == 1);
    CHECK(Editor::RulerStep(56.0) == 1);
    CHECK(Editor::RulerStep(12.0) == 5);
    CHECK(Editor::RulerStep(6.0) == 10);
    CHECK(Editor::RulerStep(2.0) == 30);
    CHECK(Editor::RulerStep(1.0) == 60);
    CHECK(Editor::RulerStep(0.2) == 300);
    CHECK(Editor::RulerStep(0.1) == 600);
    CHECK(Editor::RulerStep(0.05) == 1800);
    CHECK(Editor::RulerStep(0.001) == 1800);
}

TEST_CASE("a clip bar reads its label, or the command's own summary", "[editor][summary]") {
    Doc::Clip labelled = Draw("core_a", 0, 100, "core");
    labelled.label = "warp pose";
    CHECK(Editor::ClipSummary(labelled, "core") == "warp pose");

    Doc::Clip model = Draw("core_b", 0, 100, "core");
    model.command = Doc::ModelDraw{.asset = "scene",
                                   .model = "core",
                                   .blend_mode = Doc::ModelBlend::Additive,
                                   .spin_per_frame = {0.0, 0.10471975512, 0.0}};
    CHECK(Editor::ClipSummary(model, "core") == "core, additive, spin y 6 deg/f");

    Doc::Clip unnamed = Draw("core_d", 0, 100, "");
    unnamed.command = Doc::ModelDraw{.asset = "red", .blend_mode = Doc::ModelBlend::Additive};
    CHECK(Editor::ClipSummary(unnamed, "shield") == "shield, additive");
    CHECK(Editor::ClipSummary(unnamed, "") == "model?, additive");

    const Doc::Clip still = Draw("core_c", 0, 100, "shield");
    CHECK(Editor::ClipSummary(still, "shield") == "shield, opaque");
}

TEST_CASE("every command family summarises its own key params", "[editor][summary]") {
    CHECK(Summary(Doc::SpriteAnimate{
              .animation = "TITLE", .priority = 40, .playback = GcAnim::Playback::HoldLast}) ==
          "TITLE, hold_last, prio 40");
    CHECK(Summary(Doc::SpriteDraw{.cell = "BG_SKY",
                                  .blend = Doc::SpriteBlend::Additive,
                                  .priority = 10}) == "BG_SKY, additive, prio 10");
    CHECK(Summary(Doc::SpriteScroll{.scroll_x = -1.5, .scroll_wrap = 640.0}) ==
          "scroll x -1.5/f, wrap 640");
    CHECK(Summary(Doc::EmitterCmd{.cell = "PTC_ORAN",
                                  .spawn = Doc::Spawn::Beat,
                                  .count = 8,
                                  .radius_from = 20,
                                  .radius_to = 260}) == "PTC_ORAN x8, beat, r 20..260");
    CHECK(Summary(Doc::ModelMotionCmd{.orbit = Doc::Orbit{.radius = 2.5}, .spin_kick = 4.0}) ==
          "orbit r 2.5, kick 4");
    CHECK(Summary(Doc::CameraSet{.eye = Doc::Vec3{0.0, 1.0, -8.5}}) == "eye 0 1 -8.5");
    CHECK(Summary(Doc::LightSet{.index = 1, .enabled = false}) == "light 1, off");
    CHECK(Summary(Doc::ParamOverrideCmd{.id = "model[core].alpha", .value = 0.5}) ==
          "model[core].alpha = 0.5");
    CHECK(Summary(Doc::RngSeed{.seed = 7}) == "seed 7");
    CHECK(Summary(Doc::RhythmBeat{.rate = 3, .span = 4}) == "beat 3 / 4");
    CHECK(Summary(Doc::RhythmJitter{.span = 30, .models = {"core", "shield"}}) ==
          "jitter span 30, set, 2 model(s)");
    CHECK(Summary(Doc::OptionSelect{.option = "mode", .choice = "EXPERT"}) == "mode = EXPERT");

    Doc::Clip tween;
    tween.command = Doc::ModelTween{};
    tween.keys.push_back(Doc::Key{.at = 0});
    tween.keys.push_back(Doc::Key{.at = 30});
    CHECK(Editor::ClipSummary(tween, "core") == "model.tween, 2 key(s)");
}

TEST_CASE("a track grows a sub-lane only when a modifier sits over a primary", "[editor][lanes]") {
    Doc::Track primary_only;
    primary_only.kind = Doc::TrackKind::Model;
    primary_only.clips.push_back(Draw("a", 0, 100, "core"));
    primary_only.clips.push_back(Draw("b", 200, 300, "core"));
    CHECK(Editor::LaneCount(primary_only) == 1);
    CHECK(Editor::LaneOf(primary_only, primary_only.clips[1]) == 0);

    Doc::Clip motion;
    motion.id = "m";
    motion.start = 0;
    motion.end = 100;
    motion.command = Doc::ModelMotionCmd{};
    Doc::Clip tween;
    tween.id = "t";
    tween.start = 0;
    tween.end = 100;
    tween.command = Doc::ModelTween{};

    Doc::Track modifier_only;
    modifier_only.kind = Doc::TrackKind::Camera;
    modifier_only.clips.push_back(tween);
    CHECK(Editor::LaneCount(modifier_only) == 1);
    CHECK(Editor::LaneOf(modifier_only, modifier_only.clips[0]) == 0);

    Doc::Track emitters;
    emitters.kind = Doc::TrackKind::Fx;
    Doc::Clip emitter;
    emitter.id = "e";
    emitter.start = 0;
    emitter.end = 100;
    emitter.command = Doc::EmitterCmd{};
    emitters.clips.push_back(emitter);
    CHECK(Editor::LaneCount(emitters) == 1);
    CHECK(Editor::LaneOf(emitters, emitters.clips[0]) == 0);

    Doc::Track mixed;
    mixed.kind = Doc::TrackKind::Model;
    mixed.clips.push_back(Draw("a", 0, 100, "core"));
    mixed.clips.push_back(motion);
    CHECK(Editor::LaneCount(mixed) == 2);
    CHECK(Editor::LaneOf(mixed, mixed.clips[0]) == 0);
    CHECK(Editor::LaneOf(mixed, mixed.clips[1]) == 1);

    mixed.clips.push_back(tween);
    CHECK(Editor::LaneCount(mixed) == 3);
    CHECK(Editor::LaneOf(mixed, mixed.clips[2]) == 2);
}

TEST_CASE("every lane is tall enough for the clip label at any font size", "[editor][lanes]") {
    for (const float text : {9.0F, 13.0F, 16.0F, 21.0F, 34.0F}) {
        for (const float pad : {0.0F, 3.0F, 6.0F}) {
            const Editor::LaneMetrics metrics = Editor::LaneMetricsFor(text, pad);
            const float wanted = text + (2.0F * pad);
            CHECK(metrics.sub_clip >= wanted);
            CHECK(metrics.clip >= wanted);
            CHECK(metrics.sub_lane > metrics.sub_clip);
            CHECK(metrics.row >= metrics.clip);

            for (const bool keyed : {false, true}) {
                for (const float bar : {metrics.clip, metrics.sub_clip}) {
                    const float y = Editor::LaneLabelY(100.0F, bar, text, keyed);
                    CHECK(y >= 100.0F);
                    CHECK(y + text <= 100.0F + bar);
                }
            }
        }
    }
}

TEST_CASE("the lanes never scroll past the last track", "[editor][view]") {
    CHECK(Editor::ClampTrackScroll(400.0F, 300.0F, 180.0F) == 120.0F);
    CHECK(Editor::ClampTrackScroll(-30.0F, 300.0F, 180.0F) == 0.0F);
    CHECK(Editor::ClampTrackScroll(50.0F, 300.0F, 180.0F) == 50.0F);
    CHECK(Editor::ClampTrackScroll(50.0F, 120.0F, 180.0F) == 0.0F);
}

TEST_CASE("a clip bar's edges are resize handles and its middle moves", "[editor][drag]") {
    CHECK(Editor::ZoneAt(100.0F, 200.0F, 102.0F) == Editor::Zone::LeftHandle);
    CHECK(Editor::ZoneAt(100.0F, 200.0F, 150.0F) == Editor::Zone::Body);
    CHECK(Editor::ZoneAt(100.0F, 200.0F, 197.0F) == Editor::Zone::RightHandle);
    CHECK(Editor::ZoneAt(100.0F, 200.0F, 240.0F) == Editor::Zone::None);
    CHECK(Editor::ZoneAt(100.0F, 108.0F, 104.0F) == Editor::Zone::Body);
}

TEST_CASE("moving a clip keeps its duration", "[editor][drag]") {
    const Doc::Document document = MakeDocument();
    const Editor::DragStart drag = GrabBody(document, "core_b", 250);
    const Editor::DragResult result = Editor::ResolveDrag(document, drag, At(310, "core"));
    CHECK(result.allowed);
    CHECK(result.start == 260);
    REQUIRE(result.end.has_value());
    CHECK(*result.end == 360);
}

TEST_CASE("a move that would overlap a primary of the same family is refused", "[editor][drag]") {
    const Doc::Document document = MakeDocument();
    const Editor::DragStart drag = GrabBody(document, "core_b", 250);
    const Editor::DragResult result = Editor::ResolveDrag(document, drag, At(120, "core"));
    CHECK_FALSE(result.allowed);
}

TEST_CASE("resizing past a neighbour stops at the neighbour instead of overlapping",
          "[editor][drag]") {
    const Doc::Document document = MakeDocument();
    Editor::DragStart drag = GrabBody(document, "core_a", 100);
    drag.mode = Editor::DragMode::ResizeRight;
    const Editor::DragResult result = Editor::ResolveDrag(document, drag, At(280, "core"));
    CHECK(result.allowed);
    CHECK(result.start == 0);
    REQUIRE(result.end.has_value());
    CHECK(*result.end == 200);
}

TEST_CASE("snapping prefers the playhead over another clip's edge", "[editor][snap]") {
    const Doc::Document document = MakeDocument();
    Editor::DragInput input = At(198, "core");
    input.snap = true;
    input.playhead = 196;
    const Editor::Snap snapped = Editor::SnapFrame(document, 198, input, "core_a");
    CHECK(snapped.snapped);
    CHECK(snapped.frame == 196);
}

TEST_CASE("snapping lands a dragged edge on a neighbour's edge", "[editor][snap]") {
    const Doc::Document document = MakeDocument();
    Editor::DragInput input = At(197, "core");
    input.snap = true;
    const Editor::Snap snapped = Editor::SnapFrame(document, 197, input, "core_a");
    CHECK(snapped.snapped);
    CHECK(snapped.frame == 200);
}

TEST_CASE("alt turns snapping off and the raw frame survives", "[editor][snap]") {
    const Doc::Document document = MakeDocument();
    const Editor::Snap snapped = Editor::SnapFrame(document, 197, At(197, "core"), "core_a");
    CHECK_FALSE(snapped.snapped);
    CHECK(snapped.frame == 197);
}

TEST_CASE("a modifier clip may sit over the primary it modifies", "[editor][drag]") {
    Doc::Document document = MakeDocument();
    Doc::Clip motion;
    motion.id = "core_motion";
    motion.start = 400;
    motion.end = 450;
    motion.command = Doc::ModelMotionCmd{};
    document.tracks.front().clips.push_back(std::move(motion));

    const Editor::DragStart drag = GrabBody(document, "core_motion", 400);
    const Editor::DragResult result = Editor::ResolveDrag(document, drag, At(210, "core"));
    CHECK(result.allowed);
    CHECK(result.start == 210);
}

TEST_CASE("a ctrl-drag copy lands at the drop position and refuses its own source",
          "[editor][drag]") {
    Doc::Document document = MakeDocument();
    CHECK(Editor::OverlapsSelfCopy(document, "core", "core_a", 60, 160));
    CHECK_FALSE(Editor::OverlapsSelfCopy(document, "core", "core_a", 400, 500));
    CHECK(Editor::OverlapsSelfCopy(document, "core", "core_a", 250, 350));

    REQUIRE(Editor::DuplicateClipTo(document, "core_a", "core", "core_a_2", 400));
    const Doc::Clip* copy = Find(document, "core_a_2");
    REQUIRE(copy != nullptr);
    CHECK(copy->start == 400);
    REQUIRE(copy->end.has_value());
    CHECK(*copy->end == 500);
    CHECK(document.tracks.front().clips.size() == 3);
    CHECK(Find(document, "core_a")->start == 0);

    CHECK_FALSE(Editor::DuplicateClipTo(document, "core_a", "core", "core_a_2", 420));
    CHECK_FALSE(Editor::DuplicateClipTo(document, "core_a", "nope", "core_a_3", 420));
    REQUIRE(Editor::SetTrackLocked(document, "shield", true));
    CHECK_FALSE(Editor::DuplicateClipTo(document, "core_a", "shield", "core_a_3", 420));
}

TEST_CASE("dragging a clip onto another track of the same kind moves it there", "[editor][drag]") {
    Doc::Document document = MakeDocument();
    const Editor::DragStart drag = GrabBody(document, "core_b", 200);
    const Editor::DragResult result = Editor::ResolveDrag(document, drag, At(420, "shield"));
    CHECK(result.allowed);
    CHECK(result.track_id == "shield");
    CHECK(Editor::MoveClipToTrack(document, "core_b", "shield", result.start));
    const Editor::ClipRef moved = Editor::FindClip(document, "core_b");
    REQUIRE(moved.Valid());
    CHECK(document.tracks[(std::size_t)moved.track].id == "shield");
}

TEST_CASE("splitting at the playhead makes two clips that keep the params", "[editor][edits]") {
    Doc::Document document = MakeDocument();
    REQUIRE(Editor::SplitClip(document, "core_b", 250));
    REQUIRE(document.tracks.front().clips.size() == 3);
    const Doc::Clip* left = Find(document, "core_b");
    REQUIRE(left != nullptr);
    CHECK(left->start == 200);
    REQUIRE(left->end.has_value());
    CHECK(*left->end == 250);
    const Doc::Clip& right = document.tracks.front().clips.back();
    CHECK(right.start == 250);
    REQUIRE(right.end.has_value());
    CHECK(*right.end == 300);
    CHECK(right.id != left->id);
    CHECK(Doc::TypeOf(right.command) == Doc::CommandType::ModelDraw);
    CHECK_FALSE(Editor::SplitClip(document, "core_a", 400));
}

TEST_CASE("trimming and re-opening a clip moves only the edge asked for", "[editor][edits]") {
    Doc::Document document = MakeDocument();
    REQUIRE(Editor::TrimStart(document, "core_b", 240));
    CHECK(Find(document, "core_b")->start == 240);
    REQUIRE(Editor::TrimEnd(document, "core_b", 280));
    CHECK(*Find(document, "core_b")->end == 280);
    REQUIRE(Editor::MakeOpenEnded(document, "core_b"));
    CHECK_FALSE(Find(document, "core_b")->end.has_value());
    CHECK(Editor::ClipEnd(*Find(document, "core_b"), Editor::DocumentLength(document)) == 600);
}

TEST_CASE("muting one clip hides only that clip, muting the track hides all of them",
          "[editor][edits][eval]") {
    Doc::Document document = MakeDocument();
    REQUIRE(ModelVisible(document, 50, "core"));
    REQUIRE(ModelVisible(document, 250, "core"));

    REQUIRE(Editor::SetClipMuted(document, "core_a", true));
    CHECK_FALSE(ModelVisible(document, 50, "core"));
    CHECK(ModelVisible(document, 250, "core"));

    REQUIRE(Editor::SetTrackMuted(document, "core", true));
    CHECK_FALSE(ModelVisible(document, 250, "core"));
    CHECK(ModelVisible(document, 100, "shield"));
}

TEST_CASE("copy, paste and duplicate produce clips with fresh ids", "[editor][edits]") {
    Doc::Document document = MakeDocument();
    const std::vector<Editor::ClipboardClip> copied = Editor::CopyClips(document, {"core_a"});
    REQUIRE(copied.size() == 1);

    const std::vector<std::string> pasted = Editor::PasteClips(document, copied, 450);
    REQUIRE(pasted.size() == 1);
    CHECK(pasted[0] != "core_a");
    CHECK(Find(document, pasted[0])->start == 450);

    const std::vector<std::string> duplicated = Editor::DuplicateClips(document, {"core_a"});
    REQUIRE(duplicated.size() == 1);
    CHECK(Find(document, duplicated[0])->start == 100);

    REQUIRE(Editor::DeleteClips(document, {pasted[0], duplicated[0]}));
    CHECK(Find(document, pasted[0]) == nullptr);
    CHECK(Find(document, duplicated[0]) == nullptr);
}

TEST_CASE("undo restores the document and the selection it applied to", "[editor][undo]") {
    Editor::State editor;
    editor.LoadDocument(MakeDocument());
    CHECK_FALSE(editor.Dirty());
    CHECK(editor.UndoDepth() == 0);

    editor.Select("core_b");
    CHECK(editor.Apply([](Doc::Document& doc) { return Editor::MoveClip(doc, "core_b", 320); }));
    CHECK(editor.Dirty());
    CHECK(editor.UndoDepth() == 1);
    CHECK(Find(editor.Document(), "core_b")->start == 320);

    editor.ClearSelection();
    CHECK(editor.Undo());
    CHECK(Find(editor.Document(), "core_b")->start == 200);
    CHECK(editor.UndoDepth() == 0);
    CHECK(editor.RedoDepth() == 1);
    CHECK(editor.Selection() == std::vector<std::string>{"core_b"});
    CHECK_FALSE(editor.Dirty());

    CHECK(editor.Redo());
    CHECK(Find(editor.Document(), "core_b")->start == 320);
    CHECK(editor.Dirty());
}

TEST_CASE("a continuous drag coalesces into one undo entry", "[editor][undo]") {
    Editor::State editor;
    editor.LoadDocument(MakeDocument());

    editor.BeginGesture();
    for (int frame = 201; frame <= 260; frame++) {
        const int target = frame;
        editor.Apply(
            [target](Doc::Document& doc) { return Editor::MoveClip(doc, "core_b", target); });
    }
    editor.EndGesture();

    CHECK(Find(editor.Document(), "core_b")->start == 260);
    CHECK(editor.UndoDepth() == 1);
    CHECK(editor.Undo());
    CHECK(Find(editor.Document(), "core_b")->start == 200);
}

TEST_CASE("a new edit clears the redo stack", "[editor][undo]") {
    Editor::State editor;
    editor.LoadDocument(MakeDocument());
    editor.Apply([](Doc::Document& doc) { return Editor::MoveClip(doc, "core_b", 320); });
    REQUIRE(editor.Undo());
    REQUIRE(editor.RedoDepth() == 1);
    editor.Apply([](Doc::Document& doc) { return Editor::MoveClip(doc, "core_b", 400); });
    CHECK(editor.RedoDepth() == 0);
    CHECK(editor.UndoDepth() == 1);
}

TEST_CASE("selection clicks select, toggle and extend", "[editor][selection]") {
    Editor::State editor;
    editor.LoadDocument(MakeDocument());

    editor.Select("core_a");
    CHECK(editor.Selection() == std::vector<std::string>{"core_a"});
    editor.ExtendSelection("core_b");
    CHECK(editor.Selection().size() == 2);
    editor.ToggleSelection("core_a");
    CHECK(editor.Selection() == std::vector<std::string>{"core_b"});
    CHECK(editor.IsSelected("core_b"));
    editor.Select("core_a");
    CHECK(editor.Selection() == std::vector<std::string>{"core_a"});
    editor.ClearSelection();
    CHECK(editor.Selection().empty());
}

TEST_CASE("a double click parks a properties request that M5 consumes once", "[editor][modal]") {
    Editor::State editor;
    editor.LoadDocument(MakeDocument());
    CHECK(editor.PendingRequest().kind == Editor::RequestKind::None);

    editor.PostRequest(
        Editor::Request{.kind = Editor::RequestKind::ClipProperties, .clip_id = "core_b"});
    CHECK(editor.PendingRequest().kind == Editor::RequestKind::ClipProperties);
    CHECK(editor.PendingRequest().clip_id == "core_b");

    const Editor::Request taken = editor.TakeRequest();
    CHECK(taken.kind == Editor::RequestKind::ClipProperties);
    CHECK(taken.clip_id == "core_b");
    CHECK(editor.PendingRequest().kind == Editor::RequestKind::None);

    editor.PostRequest(
        Editor::Request{.kind = Editor::RequestKind::AddCommand, .track_id = "core", .frame = 320});
    CHECK(editor.PendingRequest().kind == Editor::RequestKind::AddCommand);
    CHECK(editor.PendingRequest().frame == 320);
}

TEST_CASE("the working document is published as an immutable snapshot", "[editor][threads]") {
    Editor::State editor;
    editor.LoadDocument(MakeDocument());
    const Editor::DocPtr before = editor.Snapshot();
    REQUIRE(before != nullptr);
    editor.Apply([](Doc::Document& doc) { return Editor::MoveClip(doc, "core_b", 320); });
    const Editor::DocPtr after = editor.Snapshot();
    CHECK(before != after);
    CHECK(Find(*before, "core_b")->start == 200);
    CHECK(Find(*after, "core_b")->start == 320);
}
