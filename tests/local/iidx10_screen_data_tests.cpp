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
#include <vector>
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

TEST_CASE("every title_10th animation draws through its whole declared length") {
    const std::string dir = Iidx10Dir();
    if (dir.empty()) SKIP("R573_IIDX10_DIR not set");

    Gc2d::Package pkg;
    std::string err;
    const std::string full =
        (std::filesystem::path(dir) / std::filesystem::path("data/graph/sys/title_10th")).string();
    REQUIRE(Gc2d::Load(full, pkg, err));

    for (const auto& [name, start] : pkg.index.animation_names) {
        const int declared = SysIdx::AnimationLength(pkg.index, start);
        INFO(name << " declares " << declared << " frames");
        REQUIRE(declared > 0);
        REQUIRE(SysIdx::AnimationContentEnd(pkg.index, start) == declared);

        std::vector<GcAnim::DrawNode> nodes;
        for (int frame = 0; frame < declared; frame++) {
            GcAnim::Evaluate(pkg.index, start, frame, 0.0F, 0.0F, nodes);
            INFO("frame " << frame);
            REQUIRE_FALSE(nodes.empty());
        }
    }
}

TEST_CASE("TITLE opens on a frame that is blank in the data, not in the exporter") {
    const std::string dir = Iidx10Dir();
    if (dir.empty()) SKIP("R573_IIDX10_DIR not set");

    Gc2d::Package pkg;
    std::string err;
    const std::string full =
        (std::filesystem::path(dir) / std::filesystem::path("data/graph/sys/title_10th")).string();
    REQUIRE(Gc2d::Load(full, pkg, err));
    const auto title = pkg.index.animation_names.find("TITLE");
    REQUIRE(title != pkg.index.animation_names.end());

    std::vector<GcAnim::DrawNode> nodes;
    GcAnim::Evaluate(pkg.index, title->second, 0, 0.0F, 0.0F, nodes);
    REQUIRE(nodes.size() == 3);
    for (const auto& node : nodes) {
        const bool offscreen = (node.y + node.h) <= 0.0F || node.y >= 480.0F;
        const bool invisible = node.alpha < 0.05F;
        INFO("node at " << node.x << "," << node.y << " alpha " << node.alpha);
        CHECK((offscreen || invisible));
    }

    GcAnim::Evaluate(pkg.index, title->second, 20, 0.0F, 0.0F, nodes);
    bool visible = false;
    for (const auto& node : nodes) {
        if (node.alpha >= 0.05F && node.y < 480.0F && (node.y + node.h) > 0.0F) visible = true;
    }
    CHECK(visible);
}
