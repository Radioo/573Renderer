#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_json.h"
#include "preset/doc/preset_validate.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace PD = Preset::Doc;

namespace {

std::string ReadFixture(const std::string& name) {
    const std::string path = std::string(R573_FIXTURE_DIR) + "/" + name;
    const std::ifstream file(path, std::ios::binary);
    REQUIRE(file.good());
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

PD::Document BaseDocument() {
    PD::Document doc;
    doc.id = "test-doc";
    doc.name = "Test document";
    doc.build = "iidx11";
    doc.length = 600;
    doc.assets.push_back(
        PD::Asset{.id = "red", .kind = PD::AssetKind::Scene3d, .dir = "data/graph/model/red"});
    return doc;
}

PD::Clip DrawClip(std::string id, int start, std::optional<int> end) {
    PD::Clip clip;
    clip.id = std::move(id);
    clip.start = start;
    clip.end = end;
    PD::ModelDraw draw;
    draw.asset = "red";
    clip.command = draw;
    return clip;
}

PD::Track ModelTrack(std::string id, std::string target, std::vector<PD::Clip> clips) {
    PD::Track track;
    track.id = std::move(id);
    track.name = target;
    track.kind = PD::TrackKind::Model;
    track.target = std::move(target);
    track.clips = std::move(clips);
    return track;
}

PD::Track SceneTrack(std::vector<PD::Clip> clips) {
    PD::Track track;
    track.id = "scene";
    track.name = "scene";
    track.kind = PD::TrackKind::Scene;
    track.clips = std::move(clips);
    return track;
}

bool Has(const std::vector<PD::Problem>& problems, PD::Severity severity, std::string_view needle) {
    return std::ranges::any_of(problems, [&](const PD::Problem& problem) {
        return problem.severity == severity && problem.message.find(needle) != std::string::npos;
    });
}

std::size_t Count(const std::vector<PD::Problem>& problems, PD::Severity severity) {
    std::size_t n = 0;
    for (const PD::Problem& problem : problems) {
        if (problem.severity == severity) ++n;
    }
    return n;
}

std::size_t CountWith(const std::vector<PD::Problem>& problems, std::string_view needle) {
    std::size_t n = 0;
    for (const PD::Problem& problem : problems) {
        if (problem.message.find(needle) != std::string::npos) ++n;
    }
    return n;
}

std::string Describe(const std::vector<PD::Problem>& problems) {
    std::string text;
    for (const PD::Problem& problem : problems) {
        text += problem.path + ": " + problem.message + "\n";
    }
    return text;
}

}

TEST_CASE("every shipped fixture validates without a problem") {
    for (const std::string& name :
         {std::string("iidx11-attract.json"), std::string("iidx10-card-in.json")}) {
        const PD::Loaded loaded = PD::Load(ReadFixture(name));
        REQUIRE(loaded.has_value());
        const std::vector<PD::Problem> problems = PD::Validate(*loaded);
        INFO(name << "\n" << Describe(problems));
        REQUIRE(problems.empty());
    }
}

TEST_CASE("two draw primaries overlapping on one target are an error") {
    PD::Document doc = BaseDocument();
    doc.tracks.push_back(
        ModelTrack("core", "core", {DrawClip("core_a", 0, 100), DrawClip("core_b", 50, 200)}));
    const std::vector<PD::Problem> problems = PD::Validate(doc);
    INFO(Describe(problems));
    REQUIRE(Has(problems, PD::Severity::Error, "overlaps"));

    PD::Document split = BaseDocument();
    split.tracks.push_back(ModelTrack("core_a_track", "core", {DrawClip("core_a", 0, 100)}));
    split.tracks.push_back(ModelTrack("core_b_track", "core", {DrawClip("core_b", 50, 200)}));
    const std::vector<PD::Problem> across = PD::Validate(split);
    INFO(Describe(across));
    REQUIRE(Has(across, PD::Severity::Error, "overlaps"));
}

TEST_CASE("a target drawn by two tracks is reported once for the pair") {
    PD::Document doc = BaseDocument();
    doc.tracks.push_back(ModelTrack("core_a_track", "core",
                                    {DrawClip("core_a1", 0, 100), DrawClip("core_a2", 100, 200)}));
    doc.tracks.push_back(ModelTrack("core_b_track", "core",
                                    {DrawClip("core_b1", 0, 100), DrawClip("core_b2", 100, 200)}));
    const std::vector<PD::Problem> problems = PD::Validate(doc);
    INFO(Describe(problems));
    REQUIRE(CountWith(problems, "second set of primary clips") == 1);
    REQUIRE(CountWith(problems, "overlaps") == 2);
}

TEST_CASE("two primaries whose gates are mutually exclusive may overlap") {
    PD::Document doc = BaseDocument();
    PD::OptionSpec option;
    option.id = "attack";
    option.label = "Attack";
    option.choices.push_back(PD::ChoiceSpec{.label = "Normal", .values = {}});
    option.choices.push_back(PD::ChoiceSpec{.label = "ATTACK", .values = {}});
    doc.options.push_back(option);

    PD::Clip normal = DrawClip("core_normal", 35, std::nullopt);
    normal.when = PD::Gate{.option = "attack", .kind = PD::GateKind::Choice, .choices = {"Normal"}};
    PD::Clip attack = DrawClip("core_attack", 35, std::nullopt);
    attack.when = PD::Gate{.option = "attack", .kind = PD::GateKind::Choice, .choices = {"ATTACK"}};
    doc.tracks.push_back(ModelTrack("core", "core", {std::move(normal), std::move(attack)}));

    const std::vector<PD::Problem> problems = PD::Validate(doc);
    INFO(Describe(problems));
    REQUIRE(problems.empty());
}

TEST_CASE("a not gate and a choices gate that share a choice are not exclusive") {
    PD::Document doc = BaseDocument();
    PD::OptionSpec option;
    option.id = "mode";
    option.label = "Mode";
    option.choices.push_back(PD::ChoiceSpec{.label = "BEGINNER", .values = {}});
    option.choices.push_back(PD::ChoiceSpec{.label = "EXPERT", .values = {}});
    option.choices.push_back(PD::ChoiceSpec{.label = "FREE", .values = {}});
    doc.options.push_back(option);

    PD::Clip wide = DrawClip("core_wide", 0, std::nullopt);
    wide.when = PD::Gate{.option = "mode", .kind = PD::GateKind::Not, .choices = {"BEGINNER"}};
    PD::Clip narrow = DrawClip("core_narrow", 0, std::nullopt);
    narrow.when = PD::Gate{
        .option = "mode", .kind = PD::GateKind::Choices, .choices = {"BEGINNER", "EXPERT"}};
    doc.tracks.push_back(ModelTrack("core", "core", {std::move(wide), std::move(narrow)}));

    const std::vector<PD::Problem> problems = PD::Validate(doc);
    INFO(Describe(problems));
    REQUIRE(Has(problems, PD::Severity::Error, "overlaps"));
}

TEST_CASE("a render.settings clip that overrides only the split priority validates") {
    PD::Document doc = BaseDocument();
    PD::Clip clip;
    clip.id = "split";
    clip.start = 0;
    clip.end = 120;
    clip.command = PD::RenderSettingsCmd{.shading = std::nullopt, .sprite_split_priority = 31};
    doc.tracks.push_back(SceneTrack({std::move(clip)}));
    const std::vector<PD::Problem> problems = PD::Validate(doc);
    INFO(Describe(problems));
    REQUIRE(problems.empty());
}

TEST_CASE("a tween modifier over its draw primary is accepted") {
    PD::Document doc = BaseDocument();
    doc.tracks.push_back(ModelTrack("core", "core", {DrawClip("core_draw", 0, std::nullopt)}));

    PD::Clip tween;
    tween.id = "core_move";
    tween.start = 10;
    tween.end = 40;
    tween.command = PD::ModelTween{};
    tween.keys.push_back(
        PD::Key{.at = 0,
                .ease = PD::Ease::Linear,
                .values = {PD::KeyValue{.id = "position", .value = PD::Vec3{0.0, 0.0, 0.0}}}});
    tween.keys.push_back(PD::Key{
        .at = 30, .values = {PD::KeyValue{.id = "position", .value = PD::Vec3{0.0, 1.0, 0.0}}}});
    doc.tracks.push_back(ModelTrack("core_tween", "core", {std::move(tween)}));

    const std::vector<PD::Problem> problems = PD::Validate(doc);
    INFO(Describe(problems));
    REQUIRE(problems.empty());
}

TEST_CASE("a tween key past the clip duration is a warning, a key before the start an error") {
    PD::Document doc = BaseDocument();
    doc.tracks.push_back(ModelTrack("core", "core", {DrawClip("core_draw", 0, std::nullopt)}));

    PD::Clip tween;
    tween.id = "core_move";
    tween.start = 10;
    tween.end = 40;
    tween.command = PD::ModelTween{};
    tween.keys.push_back(PD::Key{.at = 45, .values = {PD::KeyValue{.id = "alpha", .value = 1.0}}});
    doc.tracks.push_back(ModelTrack("core_tween", "core", {std::move(tween)}));

    const std::vector<PD::Problem> problems = PD::Validate(doc);
    INFO(Describe(problems));
    REQUIRE(Has(problems, PD::Severity::Warning, "key at 45"));
    REQUIRE_FALSE(Has(problems, PD::Severity::Error, "key at 45"));

    PD::Document early = BaseDocument();
    early.tracks.push_back(ModelTrack("core", "core", {DrawClip("core_draw", 0, std::nullopt)}));
    PD::Clip before;
    before.id = "core_early";
    before.start = 10;
    before.end = 40;
    before.command = PD::ModelTween{};
    before.keys.push_back(PD::Key{.at = -1, .values = {PD::KeyValue{.id = "alpha", .value = 1.0}}});
    early.tracks.push_back(ModelTrack("core_tween", "core", {std::move(before)}));

    const std::vector<PD::Problem> early_problems = PD::Validate(early);
    INFO(Describe(early_problems));
    REQUIRE(Has(early_problems, PD::Severity::Error, "key at -1"));
}

TEST_CASE("a tween with no draw clip under it is a warning, not an error") {
    PD::Document doc = BaseDocument();
    PD::Clip tween;
    tween.id = "core_move";
    tween.start = 10;
    tween.end = 40;
    tween.command = PD::ModelTween{};
    tween.keys.push_back(PD::Key{.at = 0, .values = {PD::KeyValue{.id = "alpha", .value = 1.0}}});
    doc.tracks.push_back(ModelTrack("core_tween", "core", {std::move(tween)}));

    const std::vector<PD::Problem> problems = PD::Validate(doc);
    INFO(Describe(problems));
    REQUIRE(Count(problems, PD::Severity::Error) == 0);
    REQUIRE(Has(problems, PD::Severity::Warning, "no model.draw"));
}

TEST_CASE("a gate naming an unknown option or an unknown choice is an error") {
    PD::Document doc = BaseDocument();
    PD::OptionSpec option;
    option.id = "mode";
    option.label = "Mode";
    option.choices.push_back(PD::ChoiceSpec{.label = "EXPERT", .values = {}});
    doc.options.push_back(option);

    PD::Clip gated = DrawClip("core_draw", 0, std::nullopt);
    gated.when = PD::Gate{.option = "attack", .kind = PD::GateKind::Choice, .choices = {"EXPERT"}};
    doc.tracks.push_back(ModelTrack("core", "core", {std::move(gated)}));
    REQUIRE(Has(PD::Validate(doc), PD::Severity::Error, "unknown option"));

    doc.tracks[0].clips[0].when =
        PD::Gate{.option = "mode", .kind = PD::GateKind::Not, .choices = {"BEGINNER"}};
    REQUIRE(Has(PD::Validate(doc), PD::Severity::Error, "unknown choice"));
}

TEST_CASE("a param.override id outside the schema grammar is an error") {
    PD::Document doc = BaseDocument();
    PD::Clip clip;
    clip.id = "override_alpha";
    clip.start = 0;
    clip.end = 100;
    clip.command = PD::ParamOverrideCmd{.id = "model[core].wobble", .value = 1.0};
    doc.tracks.push_back(SceneTrack({std::move(clip)}));
    const std::vector<PD::Problem> problems = PD::Validate(doc);
    INFO(Describe(problems));
    REQUIRE(Has(problems, PD::Severity::Error, "model[core].wobble"));

    doc.tracks[0].clips[0].command = PD::ParamOverrideCmd{.id = "model[core].alpha", .value = 0.5};
    INFO(Describe(PD::Validate(doc)));
    REQUIRE(PD::Validate(doc).empty());
}

TEST_CASE("a choice value key in the wrong grammar is an error") {
    PD::Document doc = BaseDocument();
    PD::OptionSpec option;
    option.id = "mode";
    option.label = "Mode";
    option.choices.push_back(PD::ChoiceSpec{
        .label = "EXPERT",
        .values = {PD::ChoiceValue{.id = "cube_x.position", .value = PD::Vec3{0.0, 0.0, 3.0}}}});
    doc.options.push_back(option);
    const std::vector<PD::Problem> problems = PD::Validate(doc);
    INFO(Describe(problems));
    REQUIRE(Has(problems, PD::Severity::Error, "cube_x.position"));

    doc.options[0].choices[0].values[0].id = "model[cube_x].position";
    INFO(Describe(PD::Validate(doc)));
    REQUIRE(PD::Validate(doc).empty());
}

TEST_CASE("a user id that equals a built-in id of the same build is an error") {
    const PD::Document doc = BaseDocument();
    const std::array<std::string_view, 2> builtins = {"iidx11-attract", "test-doc"};
    REQUIRE(Has(PD::Validate(doc, builtins), PD::Severity::Error, "built-in"));
    REQUIRE(PD::Validate(doc).empty());
}

TEST_CASE("a command on a track kind that does not admit it is an error") {
    PD::Document doc = BaseDocument();
    PD::Clip clip;
    clip.id = "seed";
    clip.start = 0;
    clip.command = PD::RngSeed{.seed = 7};
    doc.tracks.push_back(ModelTrack("core", "core", {std::move(clip)}));
    const std::vector<PD::Problem> problems = PD::Validate(doc);
    INFO(Describe(problems));
    REQUIRE(Has(problems, PD::Severity::Error, "rng.seed"));
}

TEST_CASE("an event clip carrying an end that differs from its start is an error") {
    PD::Document doc = BaseDocument();
    PD::Clip clip;
    clip.id = "seed";
    clip.start = 100;
    clip.end = 140;
    clip.command = PD::RngSeed{.seed = 7};
    doc.tracks.push_back(SceneTrack({std::move(clip)}));
    const std::vector<PD::Problem> problems = PD::Validate(doc);
    INFO(Describe(problems));
    REQUIRE(Has(problems, PD::Severity::Error, "event clip"));
}

TEST_CASE("a hard range is enforced and a soft range is not") {
    PD::Document doc = BaseDocument();
    PD::Track track;
    track.id = "title";
    track.name = "TITLE";
    track.kind = PD::TrackKind::Sprite;
    track.target = "TITLE";
    PD::Clip clip;
    clip.id = "title_draw";
    clip.start = 0;
    clip.end = 100;
    PD::SpriteDraw sprite;
    sprite.asset = "red";
    sprite.cell = "TITLE";
    sprite.priority = 200;
    clip.command = sprite;
    track.clips.push_back(std::move(clip));
    doc.tracks.push_back(std::move(track));
    REQUIRE(Has(PD::Validate(doc), PD::Severity::Error, "priority"));

    PD::Document soft = BaseDocument();
    PD::Track camera;
    camera.id = "cam";
    camera.name = "camera";
    camera.kind = PD::TrackKind::Camera;
    PD::Clip wide;
    wide.id = "cam_wide";
    wide.start = 0;
    wide.end = 100;
    PD::CameraSet camera_set;
    camera_set.fov_y = 100.0;
    wide.command = camera_set;
    camera.clips.push_back(std::move(wide));
    soft.tracks.push_back(std::move(camera));
    INFO(Describe(PD::Validate(soft)));
    REQUIRE(PD::Validate(soft).empty());
}

TEST_CASE("duplicate clip ids and unsorted markers are errors") {
    PD::Document doc = BaseDocument();
    doc.tracks.push_back(
        ModelTrack("core", "core", {DrawClip("core_a", 0, 100), DrawClip("core_a", 100, 200)}));
    doc.markers.push_back(PD::Marker{.frame = 100, .label = "second"});
    doc.markers.push_back(PD::Marker{.frame = 10, .label = "first"});
    const std::vector<PD::Problem> problems = PD::Validate(doc);
    INFO(Describe(problems));
    REQUIRE(Has(problems, PD::Severity::Error, "duplicate clip id"));
    REQUIRE(Has(problems, PD::Severity::Error, "markers"));
}
