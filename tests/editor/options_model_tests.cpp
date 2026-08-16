#include <catch2/catch_test_macros.hpp>

#include "editor/options_model.h"
#include "preset/doc/preset_commands.h"
#include "preset/defaults/defaults.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace Doc = Preset::Doc;

Doc::Document ModeSelect() {
    for (const Doc::Document& document : Doc::BuiltIns()) {
        if (document.id == "iidx10-mode-select") return document;
    }
    FAIL("no iidx10-mode-select built-in document");
    return {};
}

bool Contains(const std::vector<std::string>& list, const std::string& value) {
    return std::ranges::find(list, value) != list.end();
}

}

TEST_CASE("the options track summarises a transition in one line", "[editor][options]") {
    const Doc::Document document = ModeSelect();
    REQUIRE(document.options.size() == 1);
    CHECK(Editor::TransitionSummary(document.options.front().transition) ==
          "100 f at step 4, kick 15");

    const Doc::Transition plain{.frames = 60, .step = 2, .ease = Doc::Ease::EaseIn};
    CHECK(Editor::TransitionSummary(plain) == "60 f at step 2, no kick");
    CHECK(Editor::TransitionSummary(Doc::Transition{}) == "instant");
}

TEST_CASE("the runtime overlay covers frames over step document frames", "[editor][options]") {
    CHECK(Editor::TransitionSpanFrames(Doc::Transition{.frames = 100, .step = 4}) == 25);
    CHECK(Editor::TransitionSpanFrames(Doc::Transition{.frames = 100, .step = 3}) == 34);
    CHECK(Editor::TransitionSpanFrames(Doc::Transition{.frames = 0, .step = 4}) == 0);
    CHECK(Editor::TransitionSpanFrames(Doc::Transition{.frames = 100, .step = 0}) == 100);
}

TEST_CASE("a choice change moves the targets either side names", "[editor][options]") {
    const Doc::Document document = ModeSelect();
    const Doc::OptionSpec& option = document.options.front();
    const std::vector<std::string> moved = Editor::MovedTargets(option, 0, 3);
    CHECK(moved.size() == 1);
    CHECK(Contains(moved, "model[cube_x].position"));

    const std::vector<std::string> tracks = Editor::TrackIdsForTargets(document, moved);
    CHECK_FALSE(tracks.empty());
    for (const std::string& id : tracks) {
        const auto found = std::ranges::find_if(
            document.tracks, [&id](const Doc::Track& track) { return track.id == id; });
        REQUIRE(found != document.tracks.end());
        CHECK(found->target == "cube_x");
    }
}

TEST_CASE("a transition with no track of its own falls back to the options band row",
          "[editor][options]") {
    Doc::Document document;
    document.id = "camera-only";
    document.build = "iidx11";
    Doc::Track track;
    track.id = "core";
    track.kind = Doc::TrackKind::Model;
    track.target = "core";
    document.tracks.push_back(std::move(track));

    const Doc::OptionSpec option{
        .id = "view",
        .label = "View",
        .choices = {Doc::ChoiceSpec{.label = "Near",
                                    .values = {Doc::ChoiceValue{
                                        .id = "camera.eye", .value = Doc::Vec3{0.0, 0.0, 4.0}}}},
                    Doc::ChoiceSpec{.label = "Far",
                                    .values = {Doc::ChoiceValue{
                                        .id = "camera.eye", .value = Doc::Vec3{0.0, 0.0, 12.0}}}}}};

    const std::vector<std::string> moved = Editor::MovedTargets(option, 0, 1);
    REQUIRE(Contains(moved, "camera.eye"));
    CHECK(Editor::TrackIdsForTargets(document, moved) ==
          std::vector<std::string>{std::string(Editor::kOptionBandRow)});
}

TEST_CASE("the choice value picker offers the document's own targets", "[editor][options]") {
    const Doc::Document document = ModeSelect();
    const std::vector<std::string> targets = Editor::ChoiceValueTargets(document);
    CHECK(Contains(targets, "model[cube_x].position"));
    CHECK(Contains(targets, "model[cube_x].rotation"));
    CHECK(Contains(targets, "camera.eye"));
    CHECK(Contains(targets, "sprite_split_priority"));
}

TEST_CASE("a new option lands in the document with one choice", "[editor][options]") {
    Doc::Document document = ModeSelect();
    const int index = Editor::AddOption(document);
    CHECK(index == 1);
    REQUIRE(document.options.size() == 2);
    CHECK_FALSE(document.options[1].id.empty());
    CHECK(document.options[1].id != document.options[0].id);
    CHECK_FALSE(document.options[1].choices.empty());
}

TEST_CASE("reordering a choice keeps the default pointing at the same choice",
          "[editor][options]") {
    Doc::Document document = ModeSelect();
    REQUIRE(document.options.front().choices.size() >= 4);
    document.options.front().default_choice = 3;
    const std::string moved = document.options.front().choices[3].label;
    const std::string above = document.options.front().choices[2].label;

    CHECK(Editor::MoveChoice(document, 0, 3, -1));
    CHECK(document.options.front().choices[2].label == moved);
    CHECK(document.options.front().choices[3].label == above);
    CHECK(document.options.front().default_choice == 2);

    CHECK(Editor::MoveChoice(document, 0, 2, 1));
    CHECK(document.options.front().choices[3].label == moved);
    CHECK(document.options.front().choices[2].label == above);
    CHECK(document.options.front().default_choice == 3);

    const int last = (int)document.options.front().choices.size() - 1;
    CHECK_FALSE(Editor::MoveChoice(document, 0, 0, -1));
    CHECK_FALSE(Editor::MoveChoice(document, 0, last, 1));
    CHECK_FALSE(Editor::MoveChoice(document, 1, 0, 1));
}

TEST_CASE("renaming a choice rewrites every gate that named it", "[editor][options]") {
    Doc::Document document;
    document.id = "gate-test";
    document.build = "iidx11";
    document.options.push_back(Doc::OptionSpec{
        .id = "attack",
        .label = "Attack",
        .choices = {Doc::ChoiceSpec{.label = "Normal"}, Doc::ChoiceSpec{.label = "ATTACK"}}});
    Doc::Track track;
    track.id = "core";
    track.kind = Doc::TrackKind::Model;
    track.target = "core";
    Doc::Clip clip;
    clip.id = "core_attack";
    clip.command = Doc::ModelDraw{};
    clip.when = Doc::Gate{.option = "attack", .kind = Doc::GateKind::Choice, .choices = {"ATTACK"}};
    track.clips.push_back(std::move(clip));
    document.tracks.push_back(std::move(track));

    CHECK(Editor::RenameChoice(document, 0, 1, "SMASH"));
    CHECK(document.options[0].choices[1].label == "SMASH");
    const Doc::Gate gate = document.tracks[0].clips[0].when.value_or(Doc::Gate{});
    CHECK(gate.choices == std::vector<std::string>{"SMASH"});
    CHECK_FALSE(Editor::RenameChoice(document, 0, 1, "Normal"));
}
