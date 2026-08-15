#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "preset/defaults/defaults.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_json.h"
#include "preset/doc/preset_validate.h"
#include "preset/preset_asset_lengths.h"
#include "preset/preset_convert.h"
#include "preset/preset_tools.h"
#include "preset/scene_preset.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace PD = Preset::Doc;

std::string ReadFixture(const std::string& name) {
    const std::ifstream file(std::string(R573_FIXTURE_DIR) + "/" + name, std::ios::binary);
    REQUIRE(file.good());
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

Preset::AssetLengths Lengths() {
    const nlohmann::json doc = nlohmann::json::parse(ReadFixture("asset_lengths.json"));
    Preset::AssetLengths lengths;
    for (const auto& [dir, ticks] : doc["scene3d"].items())
        lengths.scene_ticks[dir] = ticks.get<float>();
    for (const auto& [dir, animations] : doc["package2d"].items()) {
        for (const auto& [name, frames] : animations.items())
            lengths.animation_frames[dir][name] = frames.get<int>();
    }
    return lengths;
}

std::vector<const Preset::Scene*> AllScenes() {
    std::vector<const Preset::Scene*> scenes;
    for (const std::string_view build : {"iidx10", "iidx11"}) {
        for (const Preset::Scene* scene : Preset::ForBuild(build))
            scenes.push_back(scene);
    }
    return scenes;
}

const PD::Document* Find(const std::vector<PD::Document>& documents, std::string_view id) {
    for (const PD::Document& document : documents) {
        if (document.id == id) return &document;
    }
    return nullptr;
}

std::string Describe(const std::vector<PD::Problem>& problems) {
    std::string out;
    for (const PD::Problem& problem : problems) {
        if (problem.severity != PD::Severity::Error) continue;
        out += "\n  error " + problem.path + ": " + problem.message;
    }
    return out;
}

}

TEST_CASE("every old scene has a built-in document equal to the converter output") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const Preset::AssetLengths lengths = Lengths();
    const std::vector<const Preset::Scene*> scenes = AllScenes();
    REQUIRE(built_ins.size() == scenes.size());
    for (const Preset::Scene* scene : scenes) {
        INFO(scene->id);
        const PD::Document* document = Find(built_ins, scene->id);
        REQUIRE(document != nullptr);
        const PD::Document converted = Preset::FromScene(*scene, lengths);
        CHECK(document->build == converted.build);
        CHECK(document->name == converted.name);
        CHECK(document->length == converted.length);
        CHECK(document->markers == converted.markers);
        CHECK(document->assets == converted.assets);
        CHECK(document->options == converted.options);
        REQUIRE(document->tracks.size() == converted.tracks.size());
        for (std::size_t i = 0; i < converted.tracks.size(); i++) {
            INFO("track " << converted.tracks[i].id);
            REQUIRE(document->tracks[i] == converted.tracks[i]);
        }
        REQUIRE(*document == converted);
        REQUIRE(PD::Save(*document) == PD::Save(converted));
    }
}

TEST_CASE("built-in documents carry unique ids and validate without an error") {
    std::set<std::string> seen;
    for (const PD::Document& document : PD::BuiltIns()) {
        INFO(document.id);
        REQUIRE_FALSE(document.id.empty());
        REQUIRE_FALSE(document.build.empty());
        REQUIRE(seen.insert(document.build + "/" + document.id).second);
        const std::vector<PD::Problem> problems = PD::Validate(document);
        INFO(Describe(problems));
        bool failed = false;
        for (const PD::Problem& problem : problems)
            failed = failed || problem.severity == PD::Severity::Error;
        REQUIRE_FALSE(failed);
    }
}

TEST_CASE("the defaults dump writes every built-in document with no window or device") {
    const std::filesystem::path out =
        std::filesystem::temp_directory_path() / "r573_dump_defaults_test";
    std::filesystem::remove_all(out);
    REQUIRE(PresetTools::DumpDefaults(out.string()) == 0);

    std::size_t written = 0;
    for (const std::filesystem::directory_entry& build : std::filesystem::directory_iterator(out)) {
        REQUIRE(build.is_directory());
        for (const std::filesystem::directory_entry& file :
             std::filesystem::directory_iterator(build.path())) {
            INFO(file.path().string());
            const std::ifstream stream(file.path(), std::ios::binary);
            REQUIRE(stream.good());
            std::ostringstream text;
            text << stream.rdbuf();
            const PD::Loaded loaded = PD::Load(text.str());
            REQUIRE(loaded.has_value());
            CHECK(loaded->build == build.path().filename().string());
            CHECK(loaded->id == file.path().stem().string());
            written++;
        }
    }
    CHECK(written == PD::BuiltIns().size());
    std::filesystem::remove_all(out);
}

TEST_CASE("the defaults dump refuses to write into an unnamed directory") {
    CHECK(PresetTools::DumpDefaults("") == 2);
}

TEST_CASE("the ending built-in merges both halves into 18 markers and unique clip ids") {
    const std::vector<PD::Document> built_ins = PD::BuiltIns();
    const PD::Document* ending = Find(built_ins, "iidx11-ending");
    REQUIRE(ending != nullptr);
    REQUIRE(ending->markers.size() == 18);
    int previous = -1;
    for (const PD::Marker& marker : ending->markers) {
        INFO(marker.label);
        REQUIRE(marker.frame > previous);
        previous = marker.frame;
    }
    std::set<std::string> clip_ids;
    std::set<std::string> track_ids;
    for (const PD::Track& track : ending->tracks) {
        INFO(track.id);
        REQUIRE(track_ids.insert(track.id).second);
        for (const PD::Clip& clip : track.clips) {
            INFO(clip.id);
            REQUIRE(clip_ids.insert(clip.id).second);
        }
    }
}
