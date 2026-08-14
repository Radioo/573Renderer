#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "game_fingerprint.h"
#include "preset/scene_preset.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

std::filesystem::path MakeFakeInstall(const std::string& leaf, std::uint64_t size, char fill) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / ("r573_fp_" + leaf);
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root / "JAE", ec);
    std::ofstream f(root / "JAE" / "bm2dx.exe", std::ios::binary);
    const std::vector<char> bytes((size_t)size, fill);
    f.write(bytes.data(), (std::streamsize)bytes.size());
    return root;
}

}

TEST_CASE("fingerprint accepts a size match as a patched copy and rejects a size mismatch") {
    const std::filesystem::path patched = MakeFakeInstall("patched", 860160, 'A');
    const GameFingerprint::Match hit = GameFingerprint::Identify(patched.string());
    REQUIRE(hit.build != nullptr);
    REQUIRE(std::string(hit.build->id) == "iidx10");
    REQUIRE(hit.file.find("bm2dx.exe") != std::string::npos);

    const std::filesystem::path wrong = MakeFakeInstall("wrongsize", 4096, 'A');
    REQUIRE(GameFingerprint::Identify(wrong.string()).build == nullptr);

    std::error_code ec;
    std::filesystem::remove_all(patched, ec);
    std::filesystem::remove_all(wrong, ec);
}

TEST_CASE("fingerprint finds nothing in an empty directory") {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "r573_fp_empty";
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    REQUIRE(GameFingerprint::Identify(root.string()).build == nullptr);
    REQUIRE(GameFingerprint::Identify("").build == nullptr);
    std::filesystem::remove_all(root, ec);
}

namespace {

const Preset::Scene& PresetById(std::string_view id) {
    for (const auto* scene : Preset::ForBuild(id.starts_with("iidx11") ? "iidx11" : "iidx10")) {
        if (scene->id == id) return *scene;
    }
    FAIL("iidx10 has no preset " << id);
    std::abort();
}

}

TEST_CASE("the iidx10 music select preset carries the values read out of the binary") {
    const Preset::Scene& scene = PresetById("iidx10-music-select");

    REQUIRE(scene.id == "iidx10-music-select");
    REQUIRE(scene.render_w == 640);
    REQUIRE(scene.render_h == 480);
    REQUIRE(scene.shading == Preset::Shading::LitMaterial);
    REQUIRE(scene.lights.size() == 2);
    REQUIRE(scene.lights[0].direction == std::array<float, 3>{1.0F, 1.0F, 1.0F});
    REQUIRE(scene.lights[1].direction == std::array<float, 3>{-1.0F, -1.0F, -1.0F});
    REQUIRE(scene.sprite_split_priority == 30);

    REQUIRE(scene.camera.eye == std::array<float, 3>{0.0F, 0.0F, -1.0F});
    REQUIRE(scene.camera.at == std::array<float, 3>{0.0F, 0.0F, 0.0F});
    REQUIRE(scene.camera.up == std::array<float, 3>{0.0F, 1.0F, 0.0F});
    REQUIRE(scene.camera.near_z == 0.1F);
    REQUIRE(scene.camera.far_z == 500.0F);

    REQUIRE(scene.countdown.start_frames == 1800);
    REQUIRE(scene.countdown.ramp_below == 600);
    REQUIRE(scene.countdown.speed_base == 0.25F);
    REQUIRE(scene.countdown.ramp_blend_mode == 3);

    REQUIRE(scene.models.size() == 1);
    REQUIRE(scene.models[0].model == "music_bg");
    REQUIRE(scene.models[0].blend_mode == 0);
    REQUIRE(scene.models[0].anim_speed == 0.25F);
    REQUIRE(scene.models[0].rotation[1] == 49.5179214F);

    REQUIRE(scene.sprites.size() == 3);
    REQUIRE(scene.sprites[0].sprite == "MU10_BG");
    REQUIRE(scene.sprites[0].priority == 31);
    REQUIRE(scene.sprites[1].sprite == "BG_SKY");
    REQUIRE(scene.sprites[1].scroll_wrap == 640.0F);
    REQUIRE(scene.sprites[2].sprite == "BG_SKY");
    REQUIRE(scene.sprites[2].x == 0.0F);

    for (const auto& sprite : scene.sprites)
        REQUIRE(sprite.priority >= scene.sprite_split_priority);
}

TEST_CASE("the card in preset inherits the music_bg model and has no ramp") {
    const Preset::Scene& scene = PresetById("iidx10-card-in");

    REQUIRE(scene.models.size() == 1);
    REQUIRE(scene.models[0].model == "music_bg");
    REQUIRE(scene.models[0].blend_mode == 3);
    REQUIRE(scene.models[0].alpha == 0.5F);
    REQUIRE(scene.models[0].anim_speed == 0.25F);
    REQUIRE(scene.models[0].rotation[1] == 0.0F);
    REQUIRE(scene.lights.size() == 2);
    REQUIRE(scene.countdown.start_frames == 3600);
    REQUIRE(scene.countdown.ramp_below == 0);

    REQUIRE(scene.sprites.size() == 1);
    REQUIRE(scene.sprites[0].sprite == "CARD_BG");
    REQUIRE(scene.sprites[0].priority == 31);
}

TEST_CASE("every registered preset names a distinct id and at least one layer") {
    std::vector<std::string> seen;
    for (const auto* scene : Preset::ForBuild("iidx10")) {
        const std::string id(scene->id);
        REQUIRE(std::ranges::find(seen, id) == seen.end());
        seen.push_back(id);
        REQUIRE_FALSE((scene->models.empty() && scene->sprites.empty()));
        REQUIRE(scene->render_w > 0);
        REQUIRE(scene->render_h > 0);
    }
    REQUIRE(seen.size() == 9);
}

TEST_CASE("the countdown ramp reproduces the values the game computes") {
    const Preset::Countdown& cd = PresetById("iidx10-music-select").countdown;
    const auto speed_at = [&cd](int remaining) {
        return cd.speed_base + ((float)(cd.ramp_below - remaining) * cd.speed_per_frame);
    };
    const auto alpha_at = [&cd](int remaining) {
        return 1.0F - ((float)(cd.ramp_below - remaining) * cd.fade_per_frame);
    };

    REQUIRE(speed_at(600) == 0.25F);
    REQUIRE(speed_at(300) == 1.0F);
    REQUIRE(alpha_at(600) == 1.0F);
    REQUIRE(alpha_at(300) < 0.81F);
    REQUIRE(alpha_at(300) > 0.79F);
    REQUIRE(speed_at(0) == 1.75F);
    REQUIRE(alpha_at(0) < 0.61F);
    REQUIRE(alpha_at(0) > 0.59F);
}

TEST_CASE("every IIDX 10 preset carries the 3D layer its screen inherits") {
    for (const auto* scene : Preset::ForBuild("iidx10")) {
        INFO("preset " << scene->id);
        REQUIRE(scene->models.size() == 1);
        REQUIRE(scene->lights.size() == 2);
        REQUIRE(scene->shading == Preset::Shading::LitMaterial);
        REQUIRE(scene->camera.eye[2] == -1.0F);
    }
}

TEST_CASE("the tumbling screens carry the motion the game recomputes each frame") {
    for (const std::string_view id : {"iidx10-dan-select", "iidx10-new-player"}) {
        const Preset::ModelMotion& motion = PresetById(id).models[0].motion;
        INFO("preset " << id);
        REQUIRE(motion.orbit_radius == 0.2F);
        REQUIRE(motion.spin_per_frame == std::array<float, 3>{-0.025F, 0.025F, 0.0125F});
        REQUIRE(motion.z_start == 100.0F);
        REQUIRE(motion.z_per_frame > 0.0F);
        REQUIRE(motion.z_min > 0.0F);
        REQUIRE(motion.z_min < motion.z_start);
    }
    REQUIRE(PresetById("iidx10-dan-select").models[0].motion.center_x == -0.55F);
    REQUIRE(PresetById("iidx10-new-player").models[0].motion.center_x == 2.23F);
}

TEST_CASE("the game over ramp fades the model out exactly as the screen ends") {
    const Preset::Scene& scene = PresetById("iidx10-game-over");
    const Preset::Countdown& cd = scene.countdown;
    REQUIRE(cd.start_frames == cd.ramp_below);
    REQUIRE(cd.fade_from == 0.8F);

    const auto at = [&cd](int remaining) {
        const auto elapsed = (float)(cd.ramp_below - remaining);
        return std::pair{cd.speed_base + (elapsed * cd.speed_per_frame),
                         cd.fade_from - (elapsed * cd.fade_per_frame)};
    };
    const auto [speed_start, alpha_start] = at(cd.start_frames);
    const auto [speed_end, alpha_end] = at(0);
    REQUIRE(speed_start == 1.6F);
    REQUIRE(alpha_start == 0.8F);
    REQUIRE(speed_end < 0.001F);
    REQUIRE(alpha_end < 0.001F);
    REQUIRE(speed_start == 2.0F * alpha_start);
}

TEST_CASE("the expert intro ramp starts the model reversed and settles forward") {
    const Preset::Scene& scene = PresetById("iidx10-expert-select");
    REQUIRE(scene.intro.frames == 22);
    REQUIRE(scene.intro.speed_from == -8.5F);
    REQUIRE(scene.intro.speed_to == scene.countdown.speed_base);
    REQUIRE(scene.countdown.speed_base == 0.5F);

    const auto speed_at = [&scene](int remaining) {
        return scene.countdown.speed_base +
               ((float)(scene.countdown.ramp_below - remaining) * scene.countdown.speed_per_frame);
    };
    REQUIRE(speed_at(600) == 0.5F);
    REQUIRE(speed_at(0) > 2.99F);
    REQUIRE(speed_at(0) < 3.01F);
}

TEST_CASE("the mode select preset exposes the game's per-mode cube placements") {
    const Preset::Scene& scene = PresetById("iidx10-mode-select");
    REQUIRE(scene.options.size() == 1);

    const Preset::Option& option = scene.options[0];
    REQUIRE(option.id == "mode");
    REQUIRE(option.choices.size() == 6);
    REQUIRE(option.transition_frames == 100);
    REQUIRE(option.choices[0].label == "BEGINNER");
    REQUIRE(option.choices[0].position == std::array<float, 3>{1.0F, 0.2F, 1.4F});
    REQUIRE(option.choices[3].label == "EXPERT");
    REQUIRE(option.choices[3].position == std::array<float, 3>{0.0F, 0.0F, 3.0F});
    REQUIRE(option.choices[5].position == std::array<float, 3>{1.0F, 0.4F, 1.0F});

    const Preset::ModelLayer& model = scene.models[0];
    REQUIRE(model.rotation[0] == -0.7853982F);
    REQUIRE(model.rotation[2] == 0.7853982F);
    REQUIRE(model.motion.spin_per_frame == std::array<float, 3>{0.0F, 0.02F, 0.0F});
    REQUIRE(model.motion.spin_kick == 15.0F);
    REQUIRE(model.motion.spin_kick_decay == 0.5F);
}

TEST_CASE("the mode select background hides the chrome baked into the backdrop") {
    const Preset::Scene& scene = PresetById("iidx10-mode-select");
    REQUIRE(scene.sprites.size() == 1);
    REQUIRE(scene.sprites[0].sprite == "MODE_BG_LOOP");
    REQUIRE(scene.sprites[0].hidden_parts.size() == 11);
    REQUIRE(scene.sprites[0].hidden_parts[0] == "FRAME");
    REQUIRE(std::ranges::find(scene.sprites[0].hidden_parts, "MODE_T") !=
            scene.sprites[0].hidden_parts.end());
    REQUIRE(std::ranges::find(scene.sprites[0].hidden_parts, "INFOWAKU") !=
            scene.sprites[0].hidden_parts.end());
}

TEST_CASE("the IIDX RED presets carry the red scene's models and its own lighting") {
    const std::vector<const Preset::Scene*> red = Preset::ForBuild("iidx11");
    REQUIRE(red.size() == 9);

    std::vector<std::string> seen;
    for (const auto* scene : red) {
        const std::string id(scene->id);
        INFO("preset " << id);
        REQUIRE(std::ranges::find(seen, id) == seen.end());
        seen.push_back(id);
        REQUIRE_FALSE(scene->models.empty());
        REQUIRE(scene->lights.size() == 2);
        REQUIRE(scene->lights[0].direction == std::array<float, 3>{1.0F, 1.0F, 1.0F});
        REQUIRE(scene->lights[1].direction == std::array<float, 3>{-1.0F, -1.0F, -1.0F});
        REQUIRE(scene->camera.near_z == 0.0F);
        REQUIRE(scene->camera.far_z == 1000.0F);
        for (const auto& model : scene->models)
            REQUIRE(model.scene_dir == "data/graph/model/red");
    }
}

TEST_CASE("IIDX RED music select keeps the projection aspect the game sets") {
    const Preset::Scene& scene = PresetById("iidx11-music-select");
    REQUIRE(scene.camera.aspect == 1.7708334F);
    REQUIRE(scene.models.size() == 4);
    REQUIRE(scene.models[0].model == "core");
    REQUIRE(scene.models[0].alpha == 0.8F);
    REQUIRE(scene.models[1].model == "shield");
    REQUIRE(scene.models[1].alpha == 0.525F);
    REQUIRE(scene.models[2].model == "flame");
    REQUIRE(scene.models[2].blend_mode == 0);
    REQUIRE(scene.models[3].model == "r_side");
    REQUIRE(scene.models[3].alpha == 0.65F);
    for (const auto& model : scene.models) {
        REQUIRE(model.position == std::array<float, 3>{-0.1F, 0.0F, -0.27555565F});
        REQUIRE(model.anim_speed == 0.75F);
    }
    REQUIRE(scene.sprites.empty());
}

TEST_CASE("every IIDX RED model carries the game's own per-frame motion") {
    for (const auto* scene : Preset::ForBuild("iidx11")) {
        INFO("preset " << scene->id);
        REQUIRE_FALSE(scene->models.empty());
        bool driven = false;
        for (const auto& model : scene->models) {
            REQUIRE(model.anim_speed > 0.0F);
            if (model.motion.spin_per_frame != std::array<float, 3>{0.0F, 0.0F, 0.0F})
                driven = true;
        }
        const std::string id(scene->id);
        const bool clip_driven =
            (id == "iidx11-card-in" || id == "iidx11-login" || id == "iidx11-new-player");
        REQUIRE(driven == !clip_driven);
    }
}

TEST_CASE("IIDX RED mode select uses the camera its update re-issues every frame") {
    const Preset::Scene& scene = PresetById("iidx11-mode-select");
    REQUIRE(scene.camera.eye == std::array<float, 3>{-0.15F, 0.14F, -0.06F});
    REQUIRE(scene.camera.at == std::array<float, 3>{1.12F, -1.31F, 1.28F});
    REQUIRE(scene.camera.aspect == 1.7708334F);
    REQUIRE(scene.models.size() == 2);
    for (const auto& model : scene.models) {
        REQUIRE(model.blend_mode == 3);
        REQUIRE(model.rotation[1] == 4.2F);
        REQUIRE(model.motion.spin_per_frame[1] == -0.008F);
    }
}

TEST_CASE("IIDX RED card in inherits the gate state the title sequence hands over") {
    const Preset::Scene& scene = PresetById("iidx11-card-in");
    REQUIRE(scene.models.size() == 1);
    REQUIRE(scene.models[0].model == "gate");
    REQUIRE(scene.models[0].alpha == 0.5F);
    REQUIRE(scene.models[0].blend_mode == 3);
    REQUIRE(scene.models[0].anim_speed == 0.25F);
    REQUIRE(scene.models[0].rotation[0] == -0.8F);
    REQUIRE(scene.countdown.start_frames == 3600);
}

TEST_CASE("IIDX RED attract spins each logo model on its own axis rate") {
    const Preset::Scene& scene = PresetById("iidx11-attract");
    REQUIRE(scene.models.size() == 4);
    REQUIRE(scene.models[3].model == "r_side");
    REQUIRE(scene.models[3].blend_mode == 0);
    REQUIRE(scene.models[3].motion.spin_per_frame[1] ==
            4.0F * scene.models[0].motion.spin_per_frame[1]);
    for (const auto& model : scene.models)
        REQUIRE(model.position == std::array<float, 3>{0.105F, 0.0F, 0.0F});
}

TEST_CASE("IIDX RED login settles to the gate alone") {
    const Preset::Scene& scene = PresetById("iidx11-login");
    REQUIRE(scene.models.size() == 1);
    REQUIRE(scene.models[0].model == "gate");
    REQUIRE(scene.models[0].alpha == 0.8F);
    REQUIRE(scene.models[0].anim_speed == 1.0F);
}

TEST_CASE("IIDX RED ending draws its background as a static cell") {
    const Preset::Scene& scene = PresetById("iidx11-ending");
    REQUIRE(scene.sprites.size() == 1);
    REQUIRE(scene.sprites[0].sprite == "END_BG1");
    REQUIRE_FALSE(scene.sprites[0].animated);
    REQUIRE(scene.sprites[0].priority == 31);
    REQUIRE(scene.camera.eye[2] == 0.249F);
}
