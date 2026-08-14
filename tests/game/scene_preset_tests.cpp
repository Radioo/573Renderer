#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "game_fingerprint.h"
#include "preset/preset_effective.h"
#include "preset/preset_params.h"
#include "preset/preset_rng.h"
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

TEST_CASE("every parameter's default is the value the preset table authors") {
    for (const auto* scene : Preset::ForBuild("iidx11")) {
        const Preset::Materialized pristine = Preset::Materialize(*scene, {}, {});
        INFO("preset " << scene->id);
        REQUIRE_FALSE(pristine.params.empty());
        for (const auto& param : pristine.params) {
            INFO("param " << param.id);
            REQUIRE(
                Preset::SameValue(Preset::ReadParam(param, pristine.effective), param.fallback));
            REQUIRE(
                Preset::SameValue(Preset::ClampValue(*param.desc, param.fallback), param.fallback));
        }
    }
}

TEST_CASE("a tweak overrides one parameter and clearing it restores the game's value") {
    const Preset::Scene& scene = PresetById("iidx11-mode-select");
    const Preset::Materialized pristine = Preset::Materialize(scene, {}, {});
    REQUIRE(pristine.effective.camera.fov_y == 1.0471976F);

    Preset::TweakSet tweaks;
    Preset::SetTweak(tweaks, "camera.fov_y", Preset::ToValue(1.2F));
    const Preset::Materialized tweaked = Preset::Materialize(scene, {}, tweaks);
    REQUIRE(tweaked.effective.camera.fov_y == 1.2F);
    REQUIRE(tweaked.effective.models.size() == pristine.effective.models.size());

    Preset::ClearTweak(tweaks, "camera.fov_y");
    REQUIRE(Preset::Materialize(scene, {}, tweaks).effective.camera.fov_y == 1.0471976F);
}

TEST_CASE("a range never makes the game's own value unreachable") {
    for (const char* build : {"iidx10", "iidx11"}) {
        for (const auto* scene : Preset::ForBuild(build)) {
            const Preset::Materialized pristine = Preset::Materialize(*scene, {}, {});
            for (const auto& param : pristine.params) {
                INFO(build << " " << scene->id << " " << param.id);
                REQUIRE(Preset::SameValue(Preset::ClampValue(*param.desc, param.fallback),
                                          param.fallback));
            }
        }
    }
}

TEST_CASE("no parameter can rewrite a layer's identity") {
    const Preset::Scene& scene = PresetById("iidx11-dan-select");
    for (const auto& param : Preset::Materialize(scene, {}, {}).params) {
        INFO("param " << param.id);
        const std::string key(param.desc->key);
        REQUIRE(key != "scene_dir");
        REQUIRE(key != "model");
        REQUIRE(key != "package_dir");
        REQUIRE(key != "sprite");
        REQUIRE(key != "hidden_parts");
    }
}

TEST_CASE("IIDX RED class course select offers every course as a camera state") {
    const Preset::Scene& scene = PresetById("iidx11-dan-select");
    REQUIRE(scene.options.size() == 1);
    REQUIRE(scene.options[0].choices.size() == 17);
    REQUIRE(scene.options[0].choices[0].label == "CLASS 7");
    REQUIRE(scene.options[0].choices[16].label == "10TH DAN");
    for (const auto& choice : scene.options[0].choices) {
        REQUIRE(choice.moves_camera);
        REQUIRE(choice.camera_eye[1] > 0.0F);
    }
}

TEST_CASE("every parameter id is unique within its preset") {
    for (const char* build : {"iidx10", "iidx11"}) {
        for (const auto* scene : Preset::ForBuild(build)) {
            const Preset::Materialized mat = Preset::Materialize(*scene, {}, {});
            std::vector<std::string> seen;
            for (const auto& param : mat.params) {
                INFO(build << " " << scene->id << " " << param.id);
                REQUIRE(std::ranges::find(seen, param.id) == seen.end());
                seen.push_back(param.id);
            }
        }
    }
}

TEST_CASE("a layer placed twice gets two addressable parameter sets") {
    const Preset::Scene& scene = PresetById("iidx10-music-select");
    REQUIRE(scene.sprites.size() == 3);
    REQUIRE(scene.sprites[1].sprite == "BG_SKY");
    REQUIRE(scene.sprites[2].sprite == "BG_SKY");

    const Preset::Materialized mat = Preset::Materialize(scene, {}, {});
    int first = 0;
    int second = 0;
    for (const auto& param : mat.params) {
        if (param.id.starts_with("sprite[BG_SKY].")) first++;
        if (param.id.starts_with("sprite[BG_SKY#2].")) second++;
    }
    REQUIRE(first > 0);
    REQUIRE(first == second);

    Preset::TweakSet tweaks;
    Preset::SetTweak(tweaks, "sprite[BG_SKY#2].x", Preset::ToValue(123.0F));
    const Preset::Effective eff = Preset::Materialize(scene, {}, tweaks).effective;
    REQUIRE(eff.sprites[1].x == 640.0F);
    REQUIRE(eff.sprites[2].x == 123.0F);
}

TEST_CASE("a parameter's group names the layer it belongs to") {
    const Preset::Materialized mat = Preset::Materialize(PresetById("iidx11-music-select"), {}, {});
    bool saw_core = false;
    for (const auto& param : mat.params) {
        if (param.id == "model[core].alpha") {
            REQUIRE(param.group == "Model core");
            saw_core = true;
        }
    }
    REQUIRE(saw_core);
}

TEST_CASE("the attract preset plays the screen's sequence, not just its settled pose") {
    const Preset::Scene& scene = PresetById("iidx11-attract");
    REQUIRE(scene.phases.size() == 5);
    REQUIRE(scene.phases[0].start_frame == 0);
    REQUIRE(scene.phases[1].start_frame == 502);
    REQUIRE(scene.phases[2].start_frame == 793);
    REQUIRE(scene.phases[3].start_frame == 902);
    REQUIRE(scene.phases[4].start_frame == 1736);

    for (const size_t phase : {(size_t)0, (size_t)2}) {
        const Preset::Materialized hidden =
            Preset::Materialize(scene, scene.phases[phase].params, {});
        for (const auto& model : hidden.effective.models)
            REQUIRE_FALSE(model.visible);
    }

    const Preset::Materialized warp = Preset::Materialize(scene, scene.phases[1].params, {});
    REQUIRE(warp.effective.intro.frames == 291);
    REQUIRE(warp.effective.intro.fov_from == 22.546017F);
    REQUIRE(warp.effective.intro.fov_to == 25.110188F);
    for (const auto& model : warp.effective.models) {
        REQUIRE(model.visible);
        REQUIRE(model.position == std::array<float, 3>{0.0F, 0.0F, -0.15F});
        REQUIRE(model.motion.spin_per_frame[1] == 0.017453292F);
    }

    const Preset::Materialized loop = Preset::Materialize(scene, scene.phases[3].params, {});
    REQUIRE(loop.effective.intro.fov_from == 0.0F);
    for (const auto& model : loop.effective.models)
        REQUIRE(model.position == std::array<float, 3>{0.105F, 0.0F, 0.0F});

    for (const auto& sprite : loop.effective.sprites)
        REQUIRE(sprite.visible == (sprite.sprite == "TITLE"));

    const Preset::Materialized standby = Preset::Materialize(scene, scene.phases[4].params, {});
    for (const auto& sprite : standby.effective.sprites)
        REQUIRE(sprite.visible == (sprite.sprite == "TITLE_TAIKI"));
}

TEST_CASE("a phase override never changes what a parameter reports as the game's value") {
    const Preset::Scene& scene = PresetById("iidx11-attract");
    const Preset::Materialized warp = Preset::Materialize(scene, scene.phases[1].params, {});
    for (const auto& param : warp.params) {
        INFO("param " << param.id);
        REQUIRE(Preset::SameValue(Preset::ReadParam(param, warp.effective), param.fallback));
    }
}

TEST_CASE("every phase override names a parameter that exists") {
    for (const char* build : {"iidx10", "iidx11"}) {
        for (const auto* scene : Preset::ForBuild(build)) {
            const Preset::Materialized mat = Preset::Materialize(*scene, {}, {});
            for (const auto& phase : scene->phases) {
                for (const auto& param : phase.params) {
                    INFO(scene->id << " phase '" << phase.label << "' sets '" << param.id << "'");
                    const auto match = std::ranges::find_if(
                        mat.params, [&param](const Preset::ParamInstance& instance) {
                            return instance.id == param.id;
                        });
                    REQUIRE(match != mat.params.end());
                }
            }
        }
    }
}

TEST_CASE("a fly-in phase carries the curve the screen actually uses") {
    const Preset::Scene& music = PresetById("iidx11-music-select");
    REQUIRE(music.phases.size() == 2);
    REQUIRE_FALSE(music.phases[0].ramps.empty());
    for (const auto& ramp : music.phases[0].ramps)
        REQUIRE(ramp.curve == Preset::Curve::Sine);

    const Preset::Scene& mode = PresetById("iidx11-mode-select");
    REQUIRE(mode.phases.size() == 4);
    for (const auto& ramp : mode.phases[0].ramps) {
        REQUIRE(ramp.curve == Preset::Curve::Linear);
        REQUIRE(ramp.from[1] > ramp.to[1]);
    }
}

TEST_CASE("every ramp names a parameter that exists") {
    for (const char* build : {"iidx10", "iidx11"}) {
        for (const auto* scene : Preset::ForBuild(build)) {
            const Preset::Materialized mat = Preset::Materialize(*scene, {}, {});
            for (const auto& phase : scene->phases) {
                for (const auto& ramp : phase.ramps) {
                    INFO(scene->id << " phase '" << phase.label << "' ramps '" << ramp.id << "'");
                    REQUIRE(std::ranges::any_of(mat.params,
                                                [&ramp](const Preset::ParamInstance& instance) {
                                                    return instance.id == ramp.id;
                                                }));
                    REQUIRE(ramp.frames > 0);
                }
            }
        }
    }
}

TEST_CASE("every state override names a parameter that exists") {
    for (const char* build : {"iidx10", "iidx11"}) {
        for (const auto* scene : Preset::ForBuild(build)) {
            const Preset::Materialized mat = Preset::Materialize(*scene, {}, {});
            for (const auto& option : scene->options) {
                for (const auto& choice : option.choices) {
                    for (const auto& param : choice.params) {
                        INFO(scene->id << " choice '" << choice.label << "' sets '" << param.id
                                       << "'");
                        REQUIRE(std::ranges::any_of(
                            mat.params, [&param](const Preset::ParamInstance& instance) {
                                return instance.id == param.id;
                            }));
                    }
                }
            }
        }
    }
}

TEST_CASE("the attract warp carries the ring emitter the screen blits") {
    const Preset::Scene& scene = PresetById("iidx11-attract");
    REQUIRE(scene.phases[1].emitters.size() == 1);
    const Preset::Emitter& ring = scene.phases[1].emitters[0];
    REQUIRE(ring.cell == "PTC_ORAN");
    REQUIRE(ring.count == 16);
    REQUIRE(ring.angle_step_deg == 22.5F);
    REQUIRE(ring.radius_from == 10);
    REQUIRE(ring.radius_to == 630);
    REQUIRE(ring.frames == 290);
    REQUIRE(ring.priority == 31);
    REQUIRE(ring.priority >= scene.sprite_split_priority);
    REQUIRE(ring.package_dir != scene.sprites.front().package_dir);
    REQUIRE_FALSE(ring.scatter);
    REQUIRE(ring.spawn == Preset::Spawn::EveryFrame);
    REQUIRE(ring.life == 60);
    REQUIRE(ring.scale == 100);
    for (const size_t phase : {(size_t)0, (size_t)2, (size_t)3, (size_t)4})
        REQUIRE(scene.phases[phase].emitters.empty());
}

TEST_CASE("the ending carries its whole phase timeline") {
    const Preset::Scene& scene = PresetById("iidx11-ending");
    REQUIRE(scene.phases.size() == 18);
    REQUIRE(scene.phases[0].start_frame == 0);
    REQUIRE(scene.phases[1].start_frame == 71);
    REQUIRE(scene.phases[6].start_frame == 1930);
    REQUIRE(scene.phases[17].start_frame == 4065);
    for (size_t i = 1; i < scene.phases.size(); i++)
        REQUIRE(scene.phases[i].start_frame > scene.phases[i - 1].start_frame);

    int bursts = 0;
    int repeats = 0;
    int beats = 0;
    for (const auto& phase : scene.phases) {
        for (const auto& emitter : phase.emitters) {
            REQUIRE(emitter.scatter);
            REQUIRE(emitter.span_x == 1280);
            REQUIRE(emitter.span_y == 960);
            REQUIRE(emitter.scale == 200);
            switch (emitter.spawn) {
            case Preset::Spawn::PhaseStart:
                REQUIRE(emitter.count == 480);
                REQUIRE(emitter.life == 45);
                REQUIRE(emitter.life_span == 0);
                bursts++;
                break;
            case Preset::Spawn::EveryFrame:
                REQUIRE(emitter.count == 32);
                REQUIRE(emitter.life_base == 48);
                REQUIRE(emitter.life_span == 48);
                repeats++;
                break;
            case Preset::Spawn::Beat:
                REQUIRE(emitter.count == 128);
                REQUIRE(emitter.beat_odd == 1);
                REQUIRE(emitter.life_base == 48);
                REQUIRE(emitter.life_span == 48);
                beats++;
                break;
            }
        }
    }
    REQUIRE(bursts == 5);
    REQUIRE(repeats == 1);
    REQUIRE(beats == 1);
}

TEST_CASE("the ending drives its pulses and jitter off the game's own beat grid") {
    const Preset::Scene& scene = PresetById("iidx11-ending");
    REQUIRE(scene.beat.rate == 155);
    REQUIRE(scene.beat.span == 3600);
    REQUIRE(scene.beat.offset_a == 70);
    REQUIRE(scene.beat.offset_b == 59);

    int pulsing = 0;
    int shaking = 0;
    for (const auto& phase : scene.phases) {
        for (const auto& param : phase.params) {
            if (param.id == "pulse.scale_odd") pulsing++;
            if (param.id == "jitter.span") shaking++;
        }
    }
    REQUIRE(pulsing == 5);
    REQUIRE(shaking == 2);
}

TEST_CASE("the game's subtractive generator reproduces its own stream") {
    Preset::Ran3 rng;
    rng.Seed(1);
    std::vector<int> first;
    for (int i = 0; i < 512; i++) {
        const int draw = rng.Next();
        REQUIRE(draw >= 0);
        REQUIRE(draw < 1000000000);
        first.push_back(draw);
    }

    rng.Seed(1);
    for (const int expected : first)
        REQUIRE(rng.Next() == expected);

    rng.Seed(2);
    int same = 0;
    for (const int expected : first) {
        if (rng.Next() == expected) same++;
    }
    REQUIRE(same < 8);
}
