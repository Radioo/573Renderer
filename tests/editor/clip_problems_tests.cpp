#include "editor/clip_problems.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_validate.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <utility>
#include <string>
#include <vector>

namespace {

namespace Doc = Preset::Doc;

Doc::Clip Clip(const std::string& id, int start, std::optional<int> end) {
    Doc::Clip clip;
    clip.id = id;
    clip.start = start;
    clip.end = end;
    clip.command = Doc::ModelDraw{.asset = "red", .model = "core"};
    return clip;
}

Doc::Document Overlapping() {
    Doc::Document document;
    document.id = "problem-test";
    document.build = "iidx11";
    document.length = 600;
    document.assets.push_back(
        Doc::Asset{.id = "red", .kind = Doc::AssetKind::Scene3d, .dir = "data/graph/model/red"});
    Doc::Track track;
    track.id = "core";
    track.name = "core";
    track.kind = Doc::TrackKind::Model;
    track.target = "core";
    track.clips.push_back(Clip("core_a", 0, 300));
    track.clips.push_back(Clip("core_b", 200, 500));
    document.tracks.push_back(std::move(track));
    return document;
}

}

TEST_CASE("an overlap marks both clips, not only the one the message is filed under",
          "[editor][problems]") {
    const std::vector<Doc::Problem> problems = Doc::Validate(Overlapping());
    REQUIRE_FALSE(problems.empty());

    const Editor::ClipProblems late = Editor::ProblemsForClip(problems, "core_b");
    const Editor::ClipProblems early = Editor::ProblemsForClip(problems, "core_a");
    CHECK(late.Failing());
    CHECK(early.Failing());
    REQUIRE(late.messages.size() == 1);
    REQUIRE(early.messages.size() == 1);
    CHECK(late.messages[0] == early.messages[0]);
    CHECK(late.messages[0].find("core_a") != std::string::npos);

    CHECK_FALSE(Editor::ProblemsForClip(problems, "core_c").Any());
    CHECK_FALSE(Editor::ProblemsForClip(problems, "").Any());
}

TEST_CASE("a warning marks a clip without claiming it failed", "[editor][problems]") {
    const std::vector<Doc::Problem> problems = {Doc::Problem{
        .severity = Doc::Severity::Warning, .path = "core_a", .related = {}, .message = "hidden"}};
    const Editor::ClipProblems marked = Editor::ProblemsForClip(problems, "core_a");
    CHECK(marked.Any());
    CHECK_FALSE(marked.Failing());
}

TEST_CASE("an error outranks a warning on the same clip", "[editor][problems]") {
    const std::vector<Doc::Problem> problems = {Doc::Problem{.severity = Doc::Severity::Warning,
                                                             .path = "core_a",
                                                             .related = {},
                                                             .message = "hidden"},
                                                Doc::Problem{.severity = Doc::Severity::Error,
                                                             .path = "core_a",
                                                             .related = {},
                                                             .message = "broken"}};
    const Editor::ClipProblems marked = Editor::ProblemsForClip(problems, "core_a");
    CHECK(marked.Failing());
    CHECK(marked.messages.size() == 2);
}

TEST_CASE("the gate row reads back the three gate forms", "[editor][problems]") {
    Doc::Clip clip = Clip("core_a", 0, 100);
    CHECK(Editor::GateText(clip) == "(always)");

    clip.when = Doc::Gate{.option = "mode", .kind = Doc::GateKind::Choice, .choices = {"7KEYS"}};
    CHECK(Editor::GateText(clip) == "mode is choice 7KEYS");

    clip.when->kind = Doc::GateKind::Choices;
    clip.when->choices = {"7KEYS", "EXPERT"};
    CHECK(Editor::GateText(clip) == "mode is one of 7KEYS, EXPERT");

    clip.when->kind = Doc::GateKind::Not;
    clip.when->choices = {"FREE"};
    CHECK(Editor::GateText(clip) == "mode is not FREE");
}
