#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "formats/gcanim.h"
#include "formats/sysidx.h"
#include "gc2d/gc_package.h"
#include "preset/scene_preset.h"
#include "support/env.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

std::string Iidx10Dir() {
    return Support::EnvVar("R573_IIDX10_DIR").value_or("");
}

int LengthOf(const std::string& game_dir, std::string_view package_dir, std::string_view sprite) {
    Gc2d::Package pkg;
    std::string err;
    const std::string full =
        (std::filesystem::path(game_dir) / std::filesystem::path(package_dir)).string();
    if (!Gc2d::Load(full, pkg, err)) return -1;
    const auto it = pkg.index.animation_names.find(std::string(sprite));
    if (it == pkg.index.animation_names.end()) return -1;
    return SysIdx::AnimationLength(pkg.index, it->second);
}

const Preset::Scene& PresetById(std::string_view id) {
    for (const auto* scene : Preset::ForBuild("iidx10")) {
        if (scene->id == id) return *scene;
    }
    throw std::runtime_error("unknown preset");
}

}

TEST_CASE("the frame counts baked into the IIDX 10 presets match the game data") {
    const std::string dir = Iidx10Dir();
    if (dir.empty()) SKIP("R573_IIDX10_DIR not set");

    const Preset::Scene& game_over = PresetById("iidx10-game-over");
    const int gameover_frames =
        LengthOf(dir, game_over.sprites[0].package_dir, game_over.sprites[0].sprite);
    INFO("GAMEOVER animation length");
    REQUIRE(gameover_frames == game_over.countdown.start_frames);

    const Preset::Scene& card_in = PresetById("iidx10-card-in");
    for (const auto& sprite : card_in.sprites) {
        if (sprite.timing.loop_end <= 0) continue;
        INFO("loop range of " << sprite.sprite);
        REQUIRE(LengthOf(dir, sprite.package_dir, sprite.sprite) == sprite.timing.loop_end);
    }
}

TEST_CASE("every sprite an IIDX 10 preset names exists in its package") {
    const std::string dir = Iidx10Dir();
    if (dir.empty()) SKIP("R573_IIDX10_DIR not set");

    for (const auto* scene : Preset::ForBuild("iidx10")) {
        for (const auto& sprite : scene->sprites) {
            if (!sprite.animated) continue;
            INFO(scene->id << " / " << sprite.sprite);
            REQUIRE(LengthOf(dir, sprite.package_dir, sprite.sprite) > 0);
        }
    }
}
