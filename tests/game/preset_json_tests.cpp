#include <catch2/catch_test_macros.hpp>

#include "formats/gcanim.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_json.h"

#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <utility>
#include <variant>

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
