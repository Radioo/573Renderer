#include <catch2/catch_test_macros.hpp>

#include "formats/gcanim.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_json.h"
#include "preset/doc/preset_validate.h"

#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

std::string ReadFixture(const std::string& name) {
    const std::string path = std::string(R573_FIXTURE_DIR) + "/" + name;
    const std::ifstream file(path, std::ios::binary);
    REQUIRE(file.good());
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

Preset::Doc::Document SmallDocument() {
    Preset::Doc::Document doc;
    doc.id = "round-trip";
    doc.name = "Round trip";
    doc.build = "iidx11";
    doc.length = 120;
    return doc;
}

}

TEST_CASE("the attract fixture loads with its markers, tracks and typed params") {
    const Preset::Doc::Loaded loaded = Preset::Doc::Load(ReadFixture("iidx11-attract.json"));
    REQUIRE(loaded.has_value());
    const Preset::Doc::Document& doc = *loaded;

    REQUIRE(doc.schema == "573renderer/scene-preset");
    REQUIRE(doc.version == 1);
    REQUIRE(doc.id == "iidx11-attract");
    REQUIRE(doc.build == "iidx11");
    REQUIRE(doc.fps == 60);
    REQUIRE(doc.length.has_value());
    REQUIRE(*doc.length == 2456);
    REQUIRE(doc.render.width == 640);
    REQUIRE(doc.render.shading == Preset::Doc::Shading::LitMaterial);
    REQUIRE(doc.render.sprite_split_priority == 30);
    REQUIRE_FALSE(doc.camera.aspect.automatic);
    REQUIRE(doc.camera.aspect.value == 1.7708334);
    REQUIRE(doc.lights.size() == 2);
    REQUIRE(doc.assets.size() == 3);
    REQUIRE(doc.assets[0].id == "red");
    REQUIRE(doc.assets[0].kind == Preset::Doc::AssetKind::Scene3d);
    REQUIRE(doc.rng_seed == 1);
    REQUIRE(doc.markers.size() == 5);
    REQUIRE(doc.markers[1].frame == 502);
    REQUIRE(doc.markers[4].label == "Standby logo, TITLE_TAIKI looping");
    REQUIRE(doc.tracks.size() == 9);

    const Preset::Doc::Track& title = doc.tracks[0];
    REQUIRE(title.kind == Preset::Doc::TrackKind::Sprite);
    REQUIRE(title.target == "TITLE");
    const auto* animate = std::get_if<Preset::Doc::SpriteAnimate>(&title.clips[0].command);
    REQUIRE(animate != nullptr);
    REQUIRE(animate->animation == "TITLE");
    REQUIRE(animate->playback == GcAnim::Playback::HoldLast);
    REQUIRE(animate->priority == 15);
    REQUIRE_FALSE(animate->clock.has_value());

    const Preset::Doc::Track& core = doc.tracks[2];
    REQUIRE(core.kind == Preset::Doc::TrackKind::Model);
    REQUIRE(core.target == "core");
    REQUIRE(core.clips.size() == 3);
    REQUIRE(core.clips[0].id == "core_warp");
    REQUIRE(core.clips[0].start == 502);
    REQUIRE(core.clips[0].end.has_value());
    REQUIRE(*core.clips[0].end == 793);
    REQUIRE_FALSE(core.clips[2].end.has_value());
    const auto* draw = std::get_if<Preset::Doc::ModelDraw>(&core.clips[0].command);
    REQUIRE(draw != nullptr);
    REQUIRE(draw->asset == "red");
    REQUIRE(draw->blend_mode == Preset::Doc::ModelBlend::Additive);
    REQUIRE(draw->anim_speed == 0.75);
    REQUIRE(draw->alpha == 1.0);
    REQUIRE(draw->position[2] == -0.15);
    REQUIRE(draw->spin_per_frame[1] == 0.017453292);

    const Preset::Doc::Clip& zoom = doc.tracks[7].clips[0];
    REQUIRE(Preset::Doc::TypeOf(zoom.command) == Preset::Doc::CommandType::CameraTween);
    REQUIRE(zoom.keys.size() == 2);
    REQUIRE(zoom.keys[0].ease == Preset::Doc::Ease::Linear);
    REQUIRE(zoom.keys[1].at == 291);
    REQUIRE(zoom.keys[0].values.size() == 1);
    REQUIRE(zoom.keys[0].values[0].id == "fov_y");
    REQUIRE(std::get<double>(zoom.keys[0].values[0].value) == 22.546017);

    const auto* emitter = std::get_if<Preset::Doc::EmitterCmd>(&doc.tracks[8].clips[0].command);
    REQUIRE(emitter != nullptr);
    REQUIRE(emitter->cell == "PTC_ORAN");
    REQUIRE(emitter->spawn == Preset::Doc::Spawn::EveryFrame);
    REQUIRE(emitter->count == 16);
    REQUIRE(emitter->reach_frames == 290);
    REQUIRE(emitter->life == 60);
    REQUIRE(emitter->blend == Preset::Doc::SpriteBlend::Additive);
    REQUIRE_FALSE(emitter->center.has_value());
}

TEST_CASE("the small iidx10 fixture keeps its auto aspect and both tracks") {
    const Preset::Doc::Loaded loaded = Preset::Doc::Load(ReadFixture("iidx10-card-in.json"));
    REQUIRE(loaded.has_value());
    const Preset::Doc::Document& doc = *loaded;

    REQUIRE(doc.id == "iidx10-card-in");
    REQUIRE(doc.build == "iidx10");
    REQUIRE(*doc.length == 3600);
    REQUIRE(doc.camera.aspect.automatic);
    REQUIRE(doc.markers.empty());
    REQUIRE(doc.options.empty());
    REQUIRE(doc.tracks.size() == 2);
    REQUIRE(doc.notes.empty());

    const auto* draw = std::get_if<Preset::Doc::ModelDraw>(&doc.tracks[1].clips[0].command);
    REQUIRE(draw != nullptr);
    REQUIRE(draw->asset == "music");
    REQUIRE(draw->alpha == 0.5);
    REQUIRE(draw->anim_speed == 0.25);
}

TEST_CASE("saving a loaded fixture reproduces the file byte for byte") {
    for (const std::string& name :
         {std::string("iidx11-attract.json"), std::string("iidx10-card-in.json"),
          std::string("unknown-keys.json")}) {
        const std::string text = ReadFixture(name);
        const Preset::Doc::Loaded loaded = Preset::Doc::Load(text);
        REQUIRE(loaded.has_value());
        REQUIRE(Preset::Doc::Save(*loaded) == text);
    }
}

TEST_CASE("a saved document parses back into an equal document") {
    const Preset::Doc::Loaded first = Preset::Doc::Load(ReadFixture("iidx11-attract.json"));
    REQUIRE(first.has_value());
    const Preset::Doc::Loaded second = Preset::Doc::Load(Preset::Doc::Save(*first));
    REQUIRE(second.has_value());
    REQUIRE(*second == *first);
}

TEST_CASE("unknown keys are preserved at the document, track, clip and param level") {
    const Preset::Doc::Loaded loaded = Preset::Doc::Load(ReadFixture("unknown-keys.json"));
    REQUIRE(loaded.has_value());
    const Preset::Doc::Document& doc = *loaded;

    REQUIRE(doc.extra.size() == 1);
    REQUIRE(doc.extra[0].key == "future_block");
    REQUIRE(doc.tracks[0].extra.size() == 1);
    REQUIRE(doc.tracks[0].extra[0].key == "future_track_flag");
    REQUIRE(doc.tracks[0].clips[0].extra.size() == 1);
    REQUIRE(doc.tracks[0].clips[0].extra[0].key == "future_clip_note");
    REQUIRE(doc.tracks[0].clips[0].params_extra.size() == 1);
    REQUIRE(doc.tracks[0].clips[0].params_extra[0].key == "future_param");
}

TEST_CASE("a track without a name takes its target, or its kind when it has none") {
    const std::string text = ReadFixture("iidx10-card-in.json");
    const std::string named = "      \"name\": \"CARD_BG\",\n";
    const std::string::size_type at = text.find(named);
    REQUIRE(at != std::string::npos);
    std::string trimmed = text;
    trimmed.erase(at, named.size());

    const Preset::Doc::Loaded loaded = Preset::Doc::Load(trimmed);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->tracks[0].name == "CARD_BG");
    REQUIRE(Preset::Doc::Save(*loaded) == text);

    const std::string attract = ReadFixture("iidx11-attract.json");
    const std::string camera_named = "      \"name\": \"camera aspect\",\n";
    const std::string::size_type camera_at = attract.find(camera_named);
    REQUIRE(camera_at != std::string::npos);
    std::string without_name = attract;
    without_name.erase(camera_at, camera_named.size());

    const Preset::Doc::Loaded camera = Preset::Doc::Load(without_name);
    REQUIRE(camera.has_value());
    REQUIRE(camera->tracks[6].id == "cam_aspect");
    REQUIRE(camera->tracks[6].name == "camera");
}

TEST_CASE("a whole number written for a param.override value is kept as a number") {
    Preset::Doc::Document doc = SmallDocument();
    Preset::Doc::Track track;
    track.id = "scene";
    track.name = "scene";
    track.kind = Preset::Doc::TrackKind::Scene;
    Preset::Doc::Clip clip;
    clip.id = "split";
    clip.start = 0;
    clip.end = 60;
    clip.command = Preset::Doc::ParamOverrideCmd{.id = "sprite_split_priority", .value = 31.0};
    track.clips.push_back(std::move(clip));
    doc.tracks.push_back(std::move(track));

    std::string text = Preset::Doc::Save(doc);
    const std::string::size_type at = text.find("\"value\": 31.0");
    REQUIRE(at != std::string::npos);
    text.replace(at, std::string("\"value\": 31.0").size(), "\"value\": 31");

    const Preset::Doc::Loaded loaded = Preset::Doc::Load(text);
    REQUIRE(loaded.has_value());
    const auto* override_cmd =
        std::get_if<Preset::Doc::ParamOverrideCmd>(&loaded->tracks[0].clips[0].command);
    REQUIRE(override_cmd != nullptr);
    REQUIRE(std::get<double>(override_cmd->value) == 31.0);
}

TEST_CASE("the ease of the last key survives a save and a load") {
    Preset::Doc::Document doc = SmallDocument();
    Preset::Doc::Track track;
    track.id = "core";
    track.name = "core";
    track.kind = Preset::Doc::TrackKind::Model;
    track.target = "core";
    Preset::Doc::Clip clip;
    clip.id = "core_fade";
    clip.start = 0;
    clip.end = 60;
    clip.command = Preset::Doc::ModelTween{};
    clip.keys.push_back(
        Preset::Doc::Key{.at = 0,
                         .ease = Preset::Doc::Ease::Linear,
                         .values = {Preset::Doc::KeyValue{.id = "alpha", .value = 0.0}}});
    clip.keys.push_back(
        Preset::Doc::Key{.at = 30,
                         .ease = Preset::Doc::Ease::Hold,
                         .values = {Preset::Doc::KeyValue{.id = "alpha", .value = 1.0}}});
    track.clips.push_back(std::move(clip));
    doc.tracks.push_back(std::move(track));

    const Preset::Doc::Loaded loaded = Preset::Doc::Load(Preset::Doc::Save(doc));
    REQUIRE(loaded.has_value());
    REQUIRE(*loaded == doc);
}

TEST_CASE("render.clear_color is omitted when black and round-trips when it is not") {
    Preset::Doc::Document doc = SmallDocument();
    REQUIRE(Preset::Doc::Save(doc).find("clear_color") == std::string::npos);

    doc.render.clear_color = {96.0 / 255.0, 96.0 / 255.0, 24.0 / 255.0};
    const std::string text = Preset::Doc::Save(doc);
    REQUIRE(text.find("clear_color") != std::string::npos);
    const Preset::Doc::Loaded loaded = Preset::Doc::Load(text);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->render.clear_color == doc.render.clear_color);
    REQUIRE(Preset::Doc::Save(*loaded) == text);
}

TEST_CASE("render.clear_color rejects a four component and an out of range value") {
    Preset::Doc::Document doc = SmallDocument();
    doc.render.clear_color = {0.5, 0.5, 0.5};
    const std::string text = Preset::Doc::Save(doc);

    std::string four = text;
    const std::string::size_type at = four.find("\"clear_color\": [");
    REQUIRE(at != std::string::npos);
    const std::string::size_type close = four.find(']', at);
    REQUIRE(close != std::string::npos);
    four.replace(at, close + 1 - at, "\"clear_color\": [0.5, 0.5, 0.5, 0.5]");
    const Preset::Doc::Loaded wide = Preset::Doc::Load(four);
    REQUIRE_FALSE(wide.has_value());
    REQUIRE(wide.error().message.find("clear_color") != std::string::npos);

    std::string big = text;
    big.replace(at, close + 1 - at, "\"clear_color\": [1.5, 0.5, 0.5]");
    const Preset::Doc::Loaded over = Preset::Doc::Load(big);
    REQUIRE_FALSE(over.has_value());
    REQUIRE(over.error().message.find("clear_color") != std::string::npos);
}

TEST_CASE("a document light writes ambient only when it is not black") {
    Preset::Doc::Document doc = SmallDocument();
    doc.lights.push_back(Preset::Doc::LightSpec{});
    REQUIRE(Preset::Doc::Save(doc).find("ambient") == std::string::npos);

    doc.lights[0].ambient = {1.0, 1.0, 1.0};
    const std::string text = Preset::Doc::Save(doc);
    REQUIRE(text.find("ambient") != std::string::npos);
    const Preset::Doc::Loaded loaded = Preset::Doc::Load(text);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->lights[0].ambient == doc.lights[0].ambient);
    REQUIRE(Preset::Doc::Save(*loaded) == text);
}

TEST_CASE("light.set carries an optional ambient through a save and a load") {
    Preset::Doc::Document doc = SmallDocument();
    Preset::Doc::Track track;
    track.id = "light0";
    track.name = "light";
    track.kind = Preset::Doc::TrackKind::Light;
    Preset::Doc::Clip clip;
    clip.id = "light0_set";
    clip.start = 0;
    clip.end = 60;
    clip.command = Preset::Doc::LightSet{.index = 0, .ambient = Preset::Doc::Vec3{1.0, 1.0, 1.0}};
    track.clips.push_back(std::move(clip));
    doc.tracks.push_back(std::move(track));

    const std::string text = Preset::Doc::Save(doc);
    REQUIRE(text.find("ambient") != std::string::npos);
    const Preset::Doc::Loaded loaded = Preset::Doc::Load(text);
    REQUIRE(loaded.has_value());
    REQUIRE(*loaded == doc);
}

TEST_CASE("a key may drive render.settings clear_color but never sprite_split_priority") {
    Preset::Doc::Document doc = SmallDocument();
    Preset::Doc::Track track;
    track.id = "scene";
    track.name = "scene";
    track.kind = Preset::Doc::TrackKind::Scene;
    Preset::Doc::Clip clip;
    clip.id = "clear_cycle";
    clip.start = 0;
    clip.end = 60;
    clip.command = Preset::Doc::RenderSettingsCmd{};
    clip.keys.push_back(
        Preset::Doc::Key{.at = 0,
                         .values = {Preset::Doc::KeyValue{.id = "clear_color",
                                                          .value = Preset::Doc::Vec3{0, 0, 0}}}});
    clip.keys.push_back(
        Preset::Doc::Key{.at = 30,
                         .values = {Preset::Doc::KeyValue{.id = "clear_color",
                                                          .value = Preset::Doc::Vec3{1, 1, 1}}}});
    track.clips.push_back(std::move(clip));
    doc.tracks.push_back(std::move(track));

    const Preset::Doc::Loaded loaded = Preset::Doc::Load(Preset::Doc::Save(doc));
    REQUIRE(loaded.has_value());
    REQUIRE(*loaded == doc);
    REQUIRE(Preset::Doc::Validate(*loaded).empty());

    Preset::Doc::Document rejected = doc;
    rejected.tracks[0].clips[0].keys[0].values[0] =
        Preset::Doc::KeyValue{.id = "sprite_split_priority", .value = 31};
    const std::vector<Preset::Doc::Problem> problems = Preset::Doc::Validate(rejected);
    REQUIRE_FALSE(problems.empty());
    REQUIRE(problems[0].message.find("not tweenable") != std::string::npos);
}

TEST_CASE("an unknown enum name is rejected and the error names the clip") {
    std::string text = ReadFixture("iidx11-attract.json");
    const std::string::size_type at = text.find("\"additive\"");
    REQUIRE(at != std::string::npos);
    text.replace(at, std::string("\"additive\"").size(), "\"glowing\"");

    const Preset::Doc::Loaded loaded = Preset::Doc::Load(text);
    REQUIRE_FALSE(loaded.has_value());
    REQUIRE(loaded.error().path == "core_warp");
    REQUIRE(loaded.error().message.find("glowing") != std::string::npos);
    REQUIRE(loaded.error().message.find("blend_mode") != std::string::npos);
}

TEST_CASE("a document from a newer schema version is rejected") {
    std::string text = ReadFixture("iidx10-card-in.json");
    const std::string::size_type at = text.find("\"version\": 1");
    REQUIRE(at != std::string::npos);
    text.replace(at, std::string("\"version\": 1").size(), "\"version\": 2");

    const Preset::Doc::Loaded loaded = Preset::Doc::Load(text);
    REQUIRE_FALSE(loaded.has_value());
    REQUIRE(loaded.error().message.find("version 2") != std::string::npos);
}

TEST_CASE("a foreign schema string is rejected") {
    std::string text = ReadFixture("iidx10-card-in.json");
    const std::string::size_type at = text.find("573renderer/scene-preset");
    REQUIRE(at != std::string::npos);
    text.replace(at, std::string("573renderer/scene-preset").size(), "someone-else/timeline");

    const Preset::Doc::Loaded loaded = Preset::Doc::Load(text);
    REQUIRE_FALSE(loaded.has_value());
    REQUIRE(loaded.error().message.find("someone-else/timeline") != std::string::npos);
}

TEST_CASE("a syntax error reports the line and column it was found on") {
    std::string text = ReadFixture("iidx10-card-in.json");
    const std::string::size_type at = text.find("\"fps\": 60,");
    REQUIRE(at != std::string::npos);
    text.replace(at, std::string("\"fps\": 60,").size(), "\"fps\": 60 60,");

    const Preset::Doc::Loaded loaded = Preset::Doc::Load(text);
    REQUIRE_FALSE(loaded.has_value());
    REQUIRE(loaded.error().line == 7);
    REQUIRE(loaded.error().column > 0);
}

namespace {

std::string WithTracks(const std::string& tracks) {
    std::string text = Preset::Doc::Save(SmallDocument());
    const std::string empty = "\"tracks\": []";
    const std::string::size_type at = text.find(empty);
    REQUIRE(at != std::string::npos);
    text.replace(at, empty.size(), "\"tracks\": " + tracks);
    return text;
}

std::string SceneTrack(const std::string& clips) {
    return R"([{"id": "scene", "kind": "scene", "clips": [)" + clips + "]}]";
}

}

TEST_CASE("a scene.fog clip round-trips through JSON and validates") {
    const std::string text = WithTracks(SceneTrack(
        R"({"id": "fog_on", "type": "scene.fog", "start": 0, "end": 120,
            "params": {"enabled": true, "color": [1.0, 0.5, 0.25],
                       "start": 55.0, "end": 62.5, "density": 0.25},
            "keys": [{"at": 0, "values": {"start": 55.0}},
                     {"at": 60, "values": {"start": 10.0}}]})"));

    const Preset::Doc::Loaded loaded = Preset::Doc::Load(text);
    if (!loaded.has_value()) FAIL(loaded.error().message);
    REQUIRE(loaded.has_value());
    REQUIRE(Preset::Doc::Validate(*loaded).empty());

    const std::string saved = Preset::Doc::Save(*loaded);
    REQUIRE(saved.find("\"scene.fog\"") != std::string::npos);
    REQUIRE(saved.find("62.5") != std::string::npos);
    REQUIRE(saved.find("0.25") != std::string::npos);
    const Preset::Doc::Loaded again = Preset::Doc::Load(saved);
    REQUIRE(again.has_value());
    REQUIRE(*again == *loaded);
    REQUIRE(Preset::Doc::Save(*again) == saved);
}

TEST_CASE("scene.fog rejects an out of range density, a key on enabled and a model track") {
    const std::string dense = WithTracks(SceneTrack(
        R"({"id": "fog_on", "type": "scene.fog", "start": 0, "end": 120,
            "params": {"density": 1.5}})"));
    const Preset::Doc::Loaded loaded = Preset::Doc::Load(dense);
    REQUIRE(loaded.has_value());
    REQUIRE_FALSE(Preset::Doc::Validate(*loaded).empty());

    const std::string keyed = WithTracks(SceneTrack(
        R"({"id": "fog_on", "type": "scene.fog", "start": 0, "end": 120,
            "keys": [{"at": 0, "values": {"enabled": false}}]})"));
    const Preset::Doc::Loaded keys = Preset::Doc::Load(keyed);
    REQUIRE(keys.has_value());
    REQUIRE_FALSE(Preset::Doc::Validate(*keys).empty());

    const std::string wrong =
        WithTracks(R"([{"id": "m", "kind": "model", "target": "core", "clips": [
            {"id": "fog_on", "type": "scene.fog", "start": 0, "end": 120}]}])");
    const Preset::Doc::Loaded placed = Preset::Doc::Load(wrong);
    REQUIRE(placed.has_value());
    REQUIRE_FALSE(Preset::Doc::Validate(*placed).empty());
}

TEST_CASE("a render.clear_cycle clip round-trips through JSON with its integer levels") {
    const std::string text = WithTracks(SceneTrack(
        R"({"id": "strobe", "type": "render.clear_cycle", "start": 0, "end": 120,
            "params": {"base": [1.0, 2.0, 3.0], "strobe_color": [48.0, 48.0, 48.0],
                       "strobe_period": 500, "strobe_window_a": 20,
                       "strobe_window_b_offset": 250, "strobe_window_b": 10,
                       "strobe_skip_every": 4, "ramp_period": 700,
                       "ramp_length": 200, "ramp_peak": 64}})"));

    const Preset::Doc::Loaded loaded = Preset::Doc::Load(text);
    if (!loaded.has_value()) FAIL(loaded.error().message);
    REQUIRE(loaded.has_value());
    REQUIRE(Preset::Doc::Validate(*loaded).empty());

    const std::string saved = Preset::Doc::Save(*loaded);
    REQUIRE(saved.find("\"render.clear_cycle\"") != std::string::npos);
    REQUIRE(saved.find("\"strobe_skip_every\": 4") != std::string::npos);
    REQUIRE(saved.find("\"ramp_peak\": 64") != std::string::npos);
    const Preset::Doc::Loaded again = Preset::Doc::Load(saved);
    REQUIRE(again.has_value());
    REQUIRE(*again == *loaded);
    REQUIRE(Preset::Doc::Save(*again) == saved);
}

TEST_CASE("camera.ease and model.ease clips round-trip through JSON with their masks") {
    const std::string text = WithTracks(
        R"([{"id": "camera", "kind": "camera", "clips": [
            {"id": "hold", "type": "camera.set", "start": 0, "end": 120,
             "params": {"eye": [0, 0, -1]}},
            {"id": "chase", "type": "camera.ease", "start": 21, "end": 120,
             "params": {"eye_target": [0, 0.55, -0.45], "at_target": [0, 0.18, 0],
                        "rate": 0.05, "eye_x": false, "at_x": false, "at_z": false}}]},
           {"id": "m", "kind": "model", "target": "core", "clips": [
            {"id": "draw", "type": "model.draw", "start": 0, "end": 120,
             "params": {"asset": "core"}},
            {"id": "grow", "type": "model.ease", "start": 0, "end": 120,
             "params": {"scale_target": [3.5, 3.5, 3.5], "alpha_target": 0.6,
                        "rate": 0.005, "mode": "linear", "start_at_target": true}}]}])");

    const Preset::Doc::Loaded loaded = Preset::Doc::Load(text);
    if (!loaded.has_value()) FAIL(loaded.error().message);
    REQUIRE(loaded.has_value());

    const std::string saved = Preset::Doc::Save(*loaded);
    REQUIRE(saved.find("\"camera.ease\"") != std::string::npos);
    REQUIRE(saved.find("\"model.ease\"") != std::string::npos);
    REQUIRE(saved.find("\"eye_x\": false") != std::string::npos);
    REQUIRE(saved.find("\"mode\": \"linear\"") != std::string::npos);
    REQUIRE(saved.find("\"start_at_target\": true") != std::string::npos);
    const Preset::Doc::Loaded again = Preset::Doc::Load(saved);
    REQUIRE(again.has_value());
    REQUIRE(*again == *loaded);
    REQUIRE(Preset::Doc::Save(*again) == saved);
}

TEST_CASE("an emitter burst block round-trips and the ease commands reject a bad rate") {
    const std::string text = WithTracks(
        R"([{"id": "fx", "kind": "fx", "clips": [
            {"id": "bubbles", "type": "emitter", "start": 59, "end": 120,
             "params": {"asset": "pkg", "cell": "AWA1", "spawn": "burst", "count": 3,
                        "priority": 27, "life_base": 150, "life_span": 100,
                        "burst": {"period_base": 10, "period_span": 5, "life_drift": 30,
                                  "rise_base": 40, "rise_step": 10, "rise_period": 60,
                                  "span_x": 640, "from_y": 480, "to_y": -20}}}]}])");
    const Preset::Doc::Loaded loaded = Preset::Doc::Load(text);
    if (!loaded.has_value()) FAIL(loaded.error().message);
    REQUIRE(loaded.has_value());
    const std::string saved = Preset::Doc::Save(*loaded);
    REQUIRE(saved.find("\"spawn\": \"burst\"") != std::string::npos);
    REQUIRE(saved.find("\"rise_base\": 40") != std::string::npos);
    REQUIRE(saved.find("\"to_y\": -20") != std::string::npos);
    const Preset::Doc::Loaded again = Preset::Doc::Load(saved);
    REQUIRE(again.has_value());
    REQUIRE(*again == *loaded);
    REQUIRE(Preset::Doc::Save(*again) == saved);

    const std::string wild = WithTracks(
        R"([{"id": "camera", "kind": "camera", "clips": [
            {"id": "chase", "type": "camera.ease", "start": 0, "end": 120,
             "params": {"rate": 4.0}}]}])");
    const Preset::Doc::Loaded fast = Preset::Doc::Load(wild);
    REQUIRE(fast.has_value());
    REQUIRE_FALSE(Preset::Doc::Validate(*fast).empty());

    const std::string misplaced = WithTracks(
        R"([{"id": "scene", "kind": "scene", "clips": [
            {"id": "grow", "type": "model.ease", "start": 0, "end": 120}]}])");
    const Preset::Doc::Loaded placed = Preset::Doc::Load(misplaced);
    REQUIRE(placed.has_value());
    REQUIRE_FALSE(Preset::Doc::Validate(*placed).empty());
}

TEST_CASE("camera.motion and poly.tile_grid clips round-trip through JSON") {
    const std::string text = WithTracks(
        R"([{"id": "camera", "kind": "camera", "clips": [
            {"id": "hold", "type": "camera.set", "start": 0, "end": 120,
             "params": {"eye": [0, 0, 0], "at": [0, 0, 1], "up": [1, 0, 0]}},
            {"id": "roll", "type": "camera.motion", "start": 0, "end": 120,
             "params": {"up_roll_deg_per_frame": 0.2}}]},
           {"id": "tiles", "kind": "poly", "clips": [
            {"id": "movie_tiles", "type": "poly.tile_grid", "start": 20, "end": 120,
             "params": {"rows": 3, "cols": 3, "lattice_amplitude": 0.1, "lattice_seed": 573,
                        "spacing": [3.0, 2.25], "depth": 5.0, "quad_scale": [3.6, 2.7],
                        "spin_rates": [1.0, 1.0, -2.0],
                        "orbit_rates": [0.0, -0.5, -0.33333334],
                        "burst_from": 90, "burst_step": 2.0, "burst_delay_per_tile": 10.0,
                        "alpha": 0.49803922,
                        "texture": {"movie": "data/movie/08ra.4"},
                        "movie_size": [304, 416], "texture_size": 512}}]}])");

    const Preset::Doc::Loaded loaded = Preset::Doc::Load(text);
    if (!loaded.has_value()) FAIL(loaded.error().message);
    REQUIRE(loaded.has_value());
    REQUIRE(Preset::Doc::Validate(*loaded).empty());

    const std::string saved = Preset::Doc::Save(*loaded);
    REQUIRE(saved.find("\"camera.motion\"") != std::string::npos);
    REQUIRE(saved.find("\"poly.tile_grid\"") != std::string::npos);
    REQUIRE(saved.find("\"kind\": \"poly\"") != std::string::npos);
    REQUIRE(saved.find("\"up_roll_deg_per_frame\": 0.2") != std::string::npos);
    REQUIRE(saved.find("\"lattice_seed\": 573") != std::string::npos);
    REQUIRE(saved.find("\"movie\": \"data/movie/08ra.4\"") != std::string::npos);
    const Preset::Doc::Loaded again = Preset::Doc::Load(saved);
    REQUIRE(again.has_value());
    REQUIRE(*again == *loaded);
    REQUIRE(Preset::Doc::Save(*again) == saved);
}

TEST_CASE("a poly.tile_grid with no texture round-trips as untextured tiles") {
    const std::string text = WithTracks(
        R"([{"id": "tiles", "kind": "poly", "clips": [
            {"id": "movie_tiles", "type": "poly.tile_grid", "start": 0, "end": 120,
             "params": {"texture": null}}]}])");
    const Preset::Doc::Loaded loaded = Preset::Doc::Load(text);
    if (!loaded.has_value()) FAIL(loaded.error().message);
    REQUIRE(loaded.has_value());
    const auto& grid =
        std::get<Preset::Doc::PolyTileGrid>(loaded->tracks.front().clips.front().command);
    CHECK_FALSE(grid.texture.has_value());
    CHECK(grid.rows == 3);
    CHECK(grid.cols == 3);

    const std::string saved = Preset::Doc::Save(*loaded);
    const Preset::Doc::Loaded again = Preset::Doc::Load(saved);
    REQUIRE(again.has_value());
    REQUIRE(*again == *loaded);
}

TEST_CASE("a poly.tile_grid only belongs on a poly track and two of them may not overlap") {
    const std::string misplaced = WithTracks(
        R"([{"id": "scene", "kind": "scene", "clips": [
            {"id": "tiles", "type": "poly.tile_grid", "start": 0, "end": 120}]}])");
    const Preset::Doc::Loaded placed = Preset::Doc::Load(misplaced);
    REQUIRE(placed.has_value());
    REQUIRE_FALSE(Preset::Doc::Validate(*placed).empty());

    const std::string doubled = WithTracks(
        R"([{"id": "tiles", "kind": "poly", "clips": [
            {"id": "one", "type": "poly.tile_grid", "start": 0, "end": 120},
            {"id": "two", "type": "poly.tile_grid", "start": 60, "end": 120}]}])");
    const Preset::Doc::Loaded twice = Preset::Doc::Load(doubled);
    REQUIRE(twice.has_value());
    REQUIRE_FALSE(Preset::Doc::Validate(*twice).empty());
}
