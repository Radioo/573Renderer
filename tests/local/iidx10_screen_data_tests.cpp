#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "formats/gcanim.h"
#include "formats/sysidx.h"
#include "gc2d/gc_package.h"
#include "preset/defaults/defaults.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "support/env.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include <string_view>

namespace {

namespace PD = Preset::Doc;

constexpr std::string_view kGameOverPackage = "data/graph/sys/gameover";
constexpr std::string_view kGameOverAnimation = "GAMEOVER";

struct Animation {
    std::string dir;
    std::string name;
};

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

std::vector<PD::Document> Iidx10Documents() {
    std::vector<PD::Document> out;
    for (PD::Document& document : PD::BuiltIns()) {
        if (document.build == "iidx10") out.push_back(std::move(document));
    }
    return out;
}

const PD::Document& DocumentById(const std::vector<PD::Document>& documents, std::string_view id) {
    for (const PD::Document& document : documents) {
        if (document.id == id) return document;
    }
    throw std::runtime_error("unknown preset");
}

std::string AssetDir(const PD::Document& document, const std::string& id) {
    for (const PD::Asset& asset : document.assets) {
        if (asset.id == id) return asset.dir;
    }
    return {};
}

std::vector<Animation> AnimationsOf(const PD::Document& document) {
    std::vector<Animation> out;
    for (const PD::Track& track : document.tracks) {
        for (const PD::Clip& clip : track.clips) {
            const auto* animate = std::get_if<PD::SpriteAnimate>(&clip.command);
            if (animate == nullptr) continue;
            out.push_back(
                Animation{.dir = AssetDir(document, animate->asset), .name = animate->animation});
        }
    }
    return out;
}

}

TEST_CASE("the game over document is exactly as long as the GAMEOVER animation") {
    const std::string dir = Iidx10Dir();
    if (dir.empty()) SKIP("R573_IIDX10_DIR not set");

    const std::vector<PD::Document> documents = Iidx10Documents();
    const PD::Document& game_over = DocumentById(documents, "iidx10-game-over");
    INFO("GAMEOVER animation length");
    REQUIRE(game_over.length.has_value());
    REQUIRE(LengthOf(dir, kGameOverPackage, kGameOverAnimation) == *game_over.length);
}

TEST_CASE("every sprite an IIDX 10 preset names exists in its package") {
    const std::string dir = Iidx10Dir();
    if (dir.empty()) SKIP("R573_IIDX10_DIR not set");

    for (const PD::Document& document : Iidx10Documents()) {
        for (const Animation& animation : AnimationsOf(document)) {
            INFO(document.id << " / " << animation.name);
            REQUIRE(LengthOf(dir, animation.dir, animation.name) > 0);
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
