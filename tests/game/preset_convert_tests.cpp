#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "preset/doc/preset_document.h"
#include "preset/doc/preset_json.h"
#include "preset/doc/preset_validate.h"
#include "preset/preset_asset_lengths.h"
#include "preset/preset_convert.h"
#include "preset/scene_preset.h"

#include <string>
#include <string_view>
#include <vector>

namespace {

namespace PD = Preset::Doc;

Preset::AssetLengths Lengths() {
    Preset::AssetLengths lengths;
    lengths.scene_ticks["data/graph/model/red"] = 240.0F;
    lengths.scene_ticks["data/graph/texture/music"] = 240.0F;
    lengths.scene_ticks["data/graph/texture/cube_x"] = 60.0F;
    lengths.scene_ticks["data/graph/texture/ex01"] = 480.0F;
    lengths.scene_ticks["data/graph/texture/tranbox"] = 120.0F;
    lengths.scene_ticks["data/graph/texture/samurai"] = 60.0F;
    lengths.animation_frames["data/graph/sys/title"]["TITLE"] = 1736;
    lengths.animation_frames["data/graph/sys/title"]["TITLE_TAIKI"] = 720;
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

const Preset::Scene& SceneById(std::string_view id) {
    for (const Preset::Scene* scene : AllScenes()) {
        if (scene->id == id) return *scene;
    }
    FAIL("no preset " << id);
    return *AllScenes().front();
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

TEST_CASE("every converted preset validates without an error") {
    for (const Preset::Scene* scene : AllScenes()) {
        const PD::Document document = Preset::FromScene(*scene, Lengths());
        const std::vector<PD::Problem> problems = PD::Validate(document);
        INFO(scene->id << Describe(problems));
        bool failed = false;
        for (const PD::Problem& problem : problems)
            failed = failed || problem.severity == PD::Severity::Error;
        REQUIRE_FALSE(failed);
        REQUIRE(document.id == scene->id);
        REQUIRE(document.build == scene->build);
        REQUIRE(document.length.has_value());
        REQUIRE(*document.length > 0);
    }
}

TEST_CASE("every converted preset round trips through JSON byte for byte") {
    for (const Preset::Scene* scene : AllScenes()) {
        const PD::Document document = Preset::FromScene(*scene, Lengths());
        const std::string text = PD::Save(document);
        const PD::Loaded loaded = PD::Load(text);
        INFO(scene->id);
        REQUIRE(loaded.has_value());
        REQUIRE(*loaded == document);
        REQUIRE(PD::Save(*loaded) == text);
    }
}

TEST_CASE("the converted attract document carries the five phase markers") {
    const PD::Document document = Preset::FromScene(SceneById("iidx11-attract"), Lengths());
    REQUIRE(document.markers.size() == 5);
    REQUIRE(document.markers[0].frame == 0);
    REQUIRE(document.markers[1].frame == 502);
    REQUIRE(document.markers[2].frame == 793);
    REQUIRE(document.markers[3].frame == 902);
    REQUIRE(document.markers[4].frame == 1736);
    REQUIRE(*document.length == 2456);
}
