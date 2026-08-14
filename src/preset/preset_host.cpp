#include "preset/preset_host.h"

#include "formats/gcanim.h"
#include "gc2d/gc_host.h"
#include "preset/scene_preset.h"
#include "scene3d/scene3d_host.h"
#include "scene3d/scene3d_render.h"
#include "support/log.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <climits>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace PresetHost {

namespace {

const Preset::Scene* g_scene = nullptr;
bool g_show_ui = false;
std::string g_game_dir;
std::string g_lead_model;
int g_countdown = 0;
int g_frame = 0;
int g_transition = 0;
float g_spin_kick = 1.0F;
std::array<float, 3> g_spin = {0.0F, 0.0F, 0.0F};
std::array<float, 3> g_transition_from = {0.0F, 0.0F, 0.0F};
std::vector<int> g_choices;
float g_speed = 1.0F;
float g_alpha = 1.0F;
int g_blend = 0;

std::string Resolve(const std::string& game_dir, std::string_view relative) {
    return (std::filesystem::path(game_dir) / std::filesystem::path(relative)).string();
}

std::array<float, 3> OrbitPosition(const Preset::ModelMotion& motion) {
    const auto t = (float)g_frame;
    const float angle = t * motion.orbit_rate;
    return {motion.center_x + (std::cos(angle) * motion.orbit_radius),
            motion.center_y + (std::sin(angle) * motion.orbit_radius),
            std::max(motion.z_min, motion.z_start - (t * motion.z_per_frame))};
}

std::array<float, 3> ChoicePosition() {
    const Preset::Option& option = g_scene->options.front();
    const auto& choices = option.choices;
    const int index = std::clamp(g_choices.front(), 0, (int)choices.size() - 1);
    const std::array<float, 3>& target = choices[(size_t)index].position;
    if (option.transition_frames <= 0 || g_transition <= 0) return target;

    const float t = 1.0F - ((float)g_transition / (float)option.transition_frames);
    std::array<float, 3> out = g_transition_from;
    for (size_t i = 0; i < out.size(); i++)
        out[i] += (target[i] - out[i]) * t;
    return out;
}

void ApplyMotion() {
    if (g_lead_model.empty()) return;
    const Preset::ModelLayer& layer = g_scene->models.front();
    const Preset::ModelMotion& motion = layer.motion;
    const bool orbits = motion.orbit_radius > 0.0F;
    const bool chooses = !g_scene->options.empty() && !g_choices.empty();
    if (!orbits && !chooses && motion.spin_kick <= 0.0F &&
        motion.spin_per_frame == std::array<float, 3>{0.0F, 0.0F, 0.0F})
        return;

    if (g_transition > 0) g_transition -= 4;
    if (g_spin_kick > 1.0F) {
        g_spin_kick = std::max(1.0F, g_spin_kick - motion.spin_kick_decay);
    } else if (g_spin_kick < -1.0F) {
        g_spin_kick = std::min(-1.0F, g_spin_kick + motion.spin_kick_decay);
    }
    for (size_t i = 0; i < g_spin.size(); i++)
        g_spin[i] += motion.spin_per_frame[i] * g_spin_kick;

    std::array<float, 3> position = layer.position;
    if (orbits) {
        position = OrbitPosition(motion);
    } else if (chooses) {
        position = ChoicePosition();
    }
    Scene3dHost::SetModelTransform(g_lead_model, position,
                                   {layer.rotation[0] + g_spin[0], layer.rotation[1] + g_spin[1],
                                    layer.rotation[2] + g_spin[2]});
}

void ApplyIntro() {
    const Preset::Intro& intro = g_scene->intro;
    if (intro.frames <= 0 || g_lead_model.empty()) return;
    const int frame = std::min(g_frame, intro.frames);
    const float t = (float)frame / (float)intro.frames;
    g_speed = intro.speed_from + ((intro.speed_to - intro.speed_from) * t);
    Scene3dHost::SetModelSpeed(g_lead_model, g_speed);
}

void Advance() {
    if (g_scene == nullptr) return;
    g_frame++;
    ApplyMotion();
    ApplyIntro();

    const Preset::Countdown& cd = g_scene->countdown;
    if (cd.start_frames <= 0) return;

    if (g_countdown > 0) g_countdown--;
    if (g_lead_model.empty() || g_countdown >= cd.ramp_below) return;

    const auto elapsed = (float)(cd.ramp_below - g_countdown);
    g_speed = cd.speed_base + (elapsed * cd.speed_per_frame);
    g_blend = cd.ramp_blend_mode;
    Scene3dHost::SetModelSpeed(g_lead_model, g_speed);
    Scene3dHost::SetModelBlendByName(g_lead_model, g_blend);
    if (cd.fade_per_frame <= 0.0F) return;
    g_alpha = std::clamp(cd.fade_from - (elapsed * cd.fade_per_frame), 0.0F, 1.0F);
    Scene3dHost::SetModelAlpha(g_lead_model, g_alpha);
}

std::vector<std::string> SkipParts(const Preset::SpriteLayer& sprite) {
    std::vector<std::string> names;
    if (g_show_ui) return names;
    names.reserve(sprite.ui_parts.size());
    for (const std::string_view part : sprite.ui_parts)
        names.emplace_back(part);
    return names;
}

void PlaceSprites(const Preset::Scene& scene) {
    std::vector<const Preset::SpriteLayer*> ordered;
    ordered.reserve(scene.sprites.size());
    for (const auto& sprite : scene.sprites) {
        if (sprite.ui && !g_show_ui) continue;
        ordered.push_back(&sprite);
    }
    std::ranges::stable_sort(ordered,
                             [](const Preset::SpriteLayer* a, const Preset::SpriteLayer* b) {
                                 return a->priority > b->priority;
                             });
    std::vector<Gc2dHost::SpritePlacement> placements;
    placements.reserve(ordered.size());
    for (const auto* sprite : ordered) {
        placements.push_back(Gc2dHost::SpritePlacement{.name = std::string(sprite->sprite),
                                                       .animated = sprite->animated,
                                                       .priority = sprite->priority,
                                                       .x = sprite->x,
                                                       .y = sprite->y,
                                                       .alpha = sprite->alpha,
                                                       .blend = (GcAnim::Blend)sprite->blend,
                                                       .timing = sprite->timing,
                                                       .skip_parts = SkipParts(*sprite),
                                                       .scroll_x = sprite->scroll_x,
                                                       .scroll_wrap = sprite->scroll_wrap});
    }
    Gc2dHost::SetSprites(std::move(placements));
}

void ResetPlayback(const Preset::Scene& scene) {
    g_countdown = scene.countdown.start_frames;
    g_frame = 0;
    g_transition = 0;
    g_spin = {0.0F, 0.0F, 0.0F};
    g_spin_kick =
        scene.models.empty() ? 1.0F : std::max(1.0F, scene.models.front().motion.spin_kick);
    g_choices.clear();
    for (const auto& option : scene.options)
        g_choices.push_back(option.default_choice);
    g_transition_from = g_choices.empty() ? std::array<float, 3>{0.0F, 0.0F, 0.0F}
                                          : scene.options.front().choices[0].position;
    g_speed = scene.models.empty() ? 1.0F : scene.models.front().anim_speed;
    g_alpha = scene.models.empty() ? 1.0F : scene.models.front().alpha;
    g_blend = scene.models.empty() ? 0 : scene.models.front().blend_mode;
}

Scene3dHost::Setup BuildSetup(const Preset::Scene& scene, std::string_view scene_dir) {
    Scene3dHost::Setup setup;
    setup.style = (scene.shading == Preset::Shading::LitMaterial)
                      ? Scene3d::RenderStyle::LitMaterial
                      : Scene3d::RenderStyle::TextureOnly;
    setup.projection.fov_y = scene.camera.fov_y;
    setup.projection.near_z = scene.camera.near_z;
    setup.projection.far_z = scene.camera.far_z;
    setup.eye = scene.camera.eye;
    setup.at = scene.camera.at;
    setup.up = scene.camera.up;
    for (const auto& light : scene.lights) {
        setup.lights.push_back(Scene3d::Light{
            .direction = light.direction, .diffuse = light.diffuse, .specular = light.specular});
    }
    for (const auto& layer : scene.models) {
        if (layer.scene_dir != scene_dir) continue;
        setup.models.push_back(Scene3dHost::ModelSetup{.model = std::string(layer.model),
                                                       .blend_mode = layer.blend_mode,
                                                       .alpha = layer.alpha,
                                                       .anim_speed = layer.anim_speed,
                                                       .position = layer.position,
                                                       .rotation = layer.rotation,
                                                       .scale = layer.scale});
    }
    return setup;
}

}

bool Load(const std::string& game_dir, const Preset::Scene& scene) {
    Unload();

    if (!scene.models.empty()) {
        const std::string_view dir = scene.models.front().scene_dir;
        const std::string full = Resolve(game_dir, dir);
        if (!Scene3dHost::LoadWithSetup(full, BuildSetup(scene, dir))) {
            LOG("Preset", "3D layer '%s' failed to load", full.c_str());
            return false;
        }
        g_lead_model = std::string(scene.models.front().model);
    }

    if (!scene.sprites.empty()) {
        const std::string full = Resolve(game_dir, scene.sprites.front().package_dir);
        if (!Gc2dHost::Load(full)) {
            LOG("Preset", "2D layer '%s' failed to load", full.c_str());
            Scene3dHost::Unload();
            return false;
        }
        PlaceSprites(scene);
    }

    g_scene = &scene;
    g_game_dir = game_dir;
    ResetPlayback(scene);
    LOG("Preset", "'%s' ready: %zu 3D layer(s), %zu 2D layer(s)", std::string(scene.name).c_str(),
        scene.models.size(), scene.sprites.size());
    return true;
}

void Unload() {
    if (g_scene == nullptr) return;
    Scene3dHost::Unload();
    Gc2dHost::Unload();
    g_scene = nullptr;
    g_lead_model.clear();
}

bool Active() {
    return g_scene != nullptr;
}

void RenderFrame(float dt) {
    if (g_scene == nullptr) return;
    const int split = g_scene->sprite_split_priority;
    Gc2dHost::DrawSprites(split, INT_MAX);
    Scene3dHost::RenderFrame(dt);
    Gc2dHost::DrawSprites(INT_MIN, split - 1);
    Gc2dHost::AdvanceSprites(dt);
    Advance();
}

int NaturalFrames() {
    if (g_scene == nullptr) return 0;
    if (g_scene->countdown.start_frames > 0) return g_scene->countdown.start_frames;
    if (g_scene->models.empty()) return 0;
    const float speed = g_scene->models.front().anim_speed;
    const float ticks = Scene3dHost::GetStatus().max_time;
    if (speed <= 0.0F || ticks <= 0.0F) return 0;
    return (int)std::lround(ticks / speed);
}

void Restart() {
    if (g_scene == nullptr) return;
    g_countdown = g_scene->countdown.start_frames;
    g_frame = 0;
    Scene3dHost::SetTime(0.0F);
    Gc2dHost::SetFrame(0);
    if (g_scene->models.empty()) return;
    const Preset::ModelLayer& lead = g_scene->models.front();
    g_speed = lead.anim_speed;
    g_alpha = lead.alpha;
    g_blend = lead.blend_mode;
    Scene3dHost::SetModelSpeed(g_lead_model, g_speed);
    Scene3dHost::SetModelAlpha(g_lead_model, g_alpha);
    Scene3dHost::SetModelBlendByName(g_lead_model, g_blend);
}

Status GetStatus() {
    Status s;
    if (g_scene == nullptr) return s;
    s.id = std::string(g_scene->id);
    s.name = std::string(g_scene->name);
    s.countdown = g_countdown;
    s.countdown_start = g_scene->countdown.start_frames;
    s.model_speed = g_speed;
    s.model_alpha = g_alpha;
    s.blend_mode = g_blend;
    s.show_ui = g_show_ui;
    s.option_choices = g_choices;
    return s;
}

void SetShowUi(bool on) {
    if (g_show_ui == on) return;
    g_show_ui = on;
    if (g_scene == nullptr) return;
    const Preset::Scene& scene = *g_scene;
    const std::string dir = g_game_dir;
    const int countdown = g_countdown;
    Load(dir, scene);
    SetCountdown(countdown);
}

void SetOption(int option, int choice) {
    if (g_scene == nullptr) return;
    if (option < 0 || (size_t)option >= g_choices.size()) return;
    const Preset::Option& spec = g_scene->options[(size_t)option];
    const int clamped = std::clamp(choice, 0, (int)spec.choices.size() - 1);
    if (clamped == g_choices[(size_t)option]) return;

    const int previous = g_choices[(size_t)option];
    g_transition_from = ChoicePosition();
    g_choices[(size_t)option] = clamped;
    g_transition = spec.transition_frames;
    const bool forward = clamped > previous;
    g_spin_kick = std::max(1.0F, spec.spin_kick);
    if (!forward) g_spin_kick = -g_spin_kick;
}

void SetCountdown(int frames) {
    if (g_scene == nullptr) return;
    g_countdown = std::clamp(frames, 0, g_scene->countdown.start_frames);
    if (g_scene->models.empty()) return;
    if (g_countdown < g_scene->countdown.ramp_below) return;
    const Preset::ModelLayer& lead = g_scene->models.front();
    g_speed = lead.anim_speed;
    g_alpha = lead.alpha;
    g_blend = lead.blend_mode;
    Scene3dHost::SetModelSpeed(g_lead_model, g_speed);
    Scene3dHost::SetModelAlpha(g_lead_model, g_alpha);
    Scene3dHost::SetModelBlendByName(g_lead_model, g_blend);
}

}
