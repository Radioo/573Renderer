#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "editor/timeline_edits.h"
#include "editor/tween_edits.h"
#include "preset/defaults/defaults.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_validate.h"
#include "preset/eval/eval_resolve.h"
#include "preset/eval/frame_state.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace Doc = Preset::Doc;

Doc::Clip Draw(const std::string& id, int start, std::optional<int> end, Doc::Vec3 position) {
    Doc::Clip clip;
    clip.id = id;
    clip.start = start;
    clip.end = end;
    clip.command = Doc::ModelDraw{
        .asset = "scene", .model = "core", .alpha = 0.5, .anim_speed = 0.75, .position = position};
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

Doc::Document Base() {
    Doc::Document document;
    document.id = "tween-test";
    document.name = "Tween test";
    document.build = "iidx11";
    document.length = 600;
    document.assets.push_back(
        Doc::Asset{.id = "scene", .kind = Doc::AssetKind::Scene3d, .dir = "data/model"});
    return document;
}

Doc::Document WithTween() {
    Doc::Document document = Base();
    document.tracks.push_back(ModelTrack("core", {Draw("core_draw", 0, 600, {0.0, 0.0, 0.0})}));

    Doc::Clip tween;
    tween.id = "core_tween";
    tween.start = 100;
    tween.end = 200;
    tween.command = Doc::ModelTween{};
    tween.keys.push_back(
        Doc::Key{.at = 0,
                 .ease = Doc::Ease::Linear,
                 .values = {Doc::KeyValue{.id = "position", .value = Doc::Vec3{0.0, 0.0, 0.0}}}});
    tween.keys.push_back(
        Doc::Key{.at = 100,
                 .ease = Doc::Ease::Linear,
                 .values = {Doc::KeyValue{.id = "position", .value = Doc::Vec3{10.0, 0.0, 0.0}}}});
    document.tracks.push_back(ModelTrack("core_tween_track", {std::move(tween)}));
    return document;
}

Preset::Eval::ModelSlot ModelAt(const Doc::Document& document, int frame, const std::string& name) {
    const std::vector<int> choices;
    const Preset::Eval::ResolveInput input{
        .document = &document, .choices = &choices, .tweens = true};
    const Preset::Eval::FrameState state = Preset::Eval::ResolveFrame(input, frame);
    for (const Preset::Eval::ModelSlot& slot : state.models) {
        if (slot.name == name) return slot;
    }
    FAIL("no model slot " << name);
    return {};
}

const Doc::Key& KeyOf(const Doc::Document& document, const std::string& clip_id, int index) {
    const Doc::Clip* clip = Editor::ClipById(document, clip_id);
    REQUIRE(clip != nullptr);
    REQUIRE((std::size_t)index < clip->keys.size());
    return clip->keys[(std::size_t)index];
}

Doc::Vec3 VectorOf(const Doc::Key& key, const std::string& field) {
    const Doc::KeyValue* value = Editor::KeyValueOf(key, field);
    REQUIRE(value != nullptr);
    const auto* vector = std::get_if<Doc::Vec3>(&value->value);
    REQUIRE(vector != nullptr);
    return *vector;
}

Doc::Document Attract() {
    for (const Doc::Document& document : Doc::BuiltIns()) {
        if (document.id == "iidx11-attract") return document;
    }
    FAIL("no iidx11-attract built-in document");
    return {};
}

int KeyCount(const Doc::Document& document, const std::string& clip_id) {
    const Doc::Clip* clip = Editor::ClipById(document, clip_id);
    return (clip == nullptr) ? 0 : (int)clip->keys.size();
}

int TrackIndexOf(const Doc::Document& document, const std::string& clip_id) {
    return Editor::FindClip(document, clip_id).track;
}

void RequireClean(const Doc::Document& document) {
    for (const Doc::Problem& problem : Doc::Validate(document)) {
        INFO(problem.path << " " << problem.related << ": " << problem.message);
        CHECK(false);
    }
}

}

TEST_CASE("adding a key captures the value the tween already resolves there",
          "[editor][tween][keys]") {
    Doc::Document document = WithTween();
    const float before = ModelAt(document, 125, "core").position[0];

    const int index = Editor::AddKeyAt(document, "core_tween", 50);
    CHECK(index == 1);
    CHECK(KeyCount(document, "core_tween") == 3);
    CHECK(KeyOf(document, "core_tween", 1).at == 50);
    CHECK(VectorOf(KeyOf(document, "core_tween", 1), "position")[0] == Catch::Approx(5.0));
    CHECK(ModelAt(document, 125, "core").position[0] == Catch::Approx(before));
    CHECK(ModelAt(document, 175, "core").position[0] == Catch::Approx(7.5));
}

TEST_CASE("a key added at a frame outside the clip is clamped to the clip span",
          "[editor][tween][keys]") {
    Doc::Document document = WithTween();
    CHECK(Editor::ClipDuration(document, "core_tween") == 100);
    CHECK(Editor::AddKeyAt(document, "core_tween", -20) == -1);
    CHECK(Editor::AddKeyAt(document, "core_tween", 400) == -1);
    CHECK(KeyCount(document, "core_tween") == 2);
}

TEST_CASE("moving a key is clamped between its neighbours", "[editor][tween][keys]") {
    Doc::Document document = WithTween();
    REQUIRE(Editor::AddKeyAt(document, "core_tween", 50) == 1);

    CHECK(Editor::MoveKey(document, "core_tween", 1, -40));
    CHECK(KeyOf(document, "core_tween", 1).at == 1);
    CHECK(Editor::MoveKey(document, "core_tween", 1, 400));
    CHECK(KeyOf(document, "core_tween", 1).at == 99);
    CHECK(Editor::MoveKey(document, "core_tween", 0, -10));
    CHECK(KeyOf(document, "core_tween", 0).at == 0);
    CHECK(Editor::MoveKey(document, "core_tween", 2, 400));
    CHECK(KeyOf(document, "core_tween", 2).at == 100);
}

TEST_CASE("deleting a key removes exactly that key", "[editor][tween][keys]") {
    Doc::Document document = WithTween();
    REQUIRE(Editor::AddKeyAt(document, "core_tween", 50) == 1);
    CHECK(Editor::DeleteKey(document, "core_tween", 1));
    CHECK(KeyCount(document, "core_tween") == 2);
    CHECK(KeyOf(document, "core_tween", 1).at == 100);
    CHECK_FALSE(Editor::DeleteKey(document, "core_tween", 7));
}

TEST_CASE("the ease of a key carries only the extra field its kind needs",
          "[editor][tween][keys]") {
    Doc::Document document = WithTween();

    CHECK(Editor::SetKeyEase(document, "core_tween", 0, Doc::Ease::SineDeg));
    CHECK(KeyOf(document, "core_tween", 0).ease == Doc::Ease::SineDeg);
    CHECK(KeyOf(document, "core_tween", 0).rate_deg.has_value());
    CHECK_FALSE(KeyOf(document, "core_tween", 0).cp.has_value());

    CHECK(Editor::SetKeyRate(document, "core_tween", 0, 3.0));
    CHECK(KeyOf(document, "core_tween", 0).rate_deg.value_or(0.0) == Catch::Approx(3.0));

    CHECK(Editor::SetKeyEase(document, "core_tween", 0, Doc::Ease::Bezier));
    CHECK_FALSE(KeyOf(document, "core_tween", 0).rate_deg.has_value());
    REQUIRE(KeyOf(document, "core_tween", 0).cp.has_value());
    CHECK(Editor::SetKeyBezier(document, "core_tween", 0, {0.25, 0.1, 0.75, 0.9}));
    CHECK((*KeyOf(document, "core_tween", 0).cp)[2] == Catch::Approx(0.75));

    CHECK(Editor::SetKeyEase(document, "core_tween", 0, Doc::Ease::Linear));
    CHECK_FALSE(KeyOf(document, "core_tween", 0).cp.has_value());
}

TEST_CASE("a value can be added to and removed from a key", "[editor][tween][keys]") {
    Doc::Document document = WithTween();
    const std::vector<Editor::TweenField> fields = Editor::TweenFields(document, "core_tween");
    CHECK(std::ranges::any_of(fields, [](const Editor::TweenField& field) {
        return field.id == "alpha" && field.target_id == "model[core].alpha";
    }));

    const std::optional<Doc::ParamValue> resolved =
        Editor::ResolvedFieldValue(document, "core_tween", "alpha", 150);
    REQUIRE(resolved.has_value());
    CHECK(std::get<double>(*resolved) == Catch::Approx(0.5));

    CHECK(Editor::SetKeyValue(document, "core_tween", 1, "alpha", *resolved));
    CHECK(Editor::KeyValueOf(KeyOf(document, "core_tween", 1), "alpha") != nullptr);
    CHECK(Editor::UnsetKeyValue(document, "core_tween", 1, "alpha"));
    CHECK(Editor::KeyValueOf(KeyOf(document, "core_tween", 1), "alpha") == nullptr);
}

TEST_CASE("add transition between abutting draw clips spans both sides of the cut",
          "[editor][tween][transition]") {
    Doc::Document document = Base();
    document.tracks.push_back(ModelTrack("core", {Draw("core_a", 0, 100, {0.0, 0.0, 0.0}),
                                                  Draw("core_b", 100, 300, {10.0, 0.0, 0.0})}));

    CHECK(Editor::NextDrawClip(document, "core_a") == "core_b");
    const Editor::TransitionInsert inserted =
        Editor::AddTransition(document, "core_a", "core_b", Editor::kTransitionFrames);
    REQUIRE(inserted.Valid());
    CHECK(inserted.bridge_id.empty());

    const Doc::Clip* tween = Editor::ClipById(document, inserted.tween_id);
    REQUIRE(tween != nullptr);
    CHECK(tween->start == 70);
    CHECK(tween->end.value_or(0) == 130);
    CHECK(Doc::TypeOf(tween->command) == Doc::CommandType::ModelTween);
    CHECK(TrackIndexOf(document, inserted.tween_id) == TrackIndexOf(document, "core_a") + 1);

    REQUIRE(tween->keys.size() == 2);
    CHECK(tween->keys[0].at == 0);
    CHECK(tween->keys[0].ease == Doc::Ease::Linear);
    CHECK(VectorOf(tween->keys[0], "position")[0] == Catch::Approx(0.0));
    CHECK(Editor::KeyValueOf(tween->keys[0], "alpha") != nullptr);
    CHECK(tween->keys[1].at == 60);
    CHECK(VectorOf(tween->keys[1], "position")[0] == Catch::Approx(10.0));
    CHECK(ModelAt(document, 100, "core").position[0] == Catch::Approx(5.0));
    RequireClean(document);
}

TEST_CASE("add transition across a hidden gap bridges the draw clip and holds the mid pose",
          "[editor][tween][transition]") {
    Doc::Document document = Attract();
    const std::string a = "core_warp_in_rotating_and_zooming";
    const std::string b = "core_attract_loop_settled_to_the_right";
    CHECK_FALSE(ModelAt(document, 847, "core").visible);

    CHECK(Editor::NextDrawClip(document, a) == b);
    const Editor::TransitionInsert inserted =
        Editor::AddTransition(document, a, b, Editor::kTransitionFrames);
    REQUIRE(inserted.Valid());
    CHECK_FALSE(inserted.bridge_id.empty());

    const Doc::Clip* tween = Editor::ClipById(document, inserted.tween_id);
    REQUIRE(tween != nullptr);
    CHECK(tween->start == 793);
    CHECK(tween->end.value_or(0) == 932);

    const Doc::Clip* bridge = Editor::ClipById(document, inserted.bridge_id);
    REQUIRE(bridge != nullptr);
    CHECK(bridge->start == 793);
    CHECK(bridge->end.value_or(0) == 902);

    const Preset::Eval::ModelSlot mid = ModelAt(document, 847, "core");
    CHECK(mid.visible);
    CHECK(mid.position[0] > 0.0F);
    CHECK(mid.position[0] < 0.105F);
    CHECK(mid.position[2] > -0.15F);
    CHECK(mid.position[2] < 0.0F);
    RequireClean(document);
}
