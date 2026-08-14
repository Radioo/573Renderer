#include "preset/preset_host.h"

#include "preset/preset_params.h"
#include "preset/preset_effective.h"

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
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace PresetHost {

namespace {

const Preset::Scene* g_scene = nullptr;
Preset::Materialized g_mat;
Preset::TweakSet g_tweaks;

const Preset::Effective& g_eff() {
    return g_mat.effective;
}
std::string g_game_dir;
std::string g_lead_model;
int g_countdown = 0;
int g_frame = 0;
int g_transition = 0;
float g_spin_kick = 1.0F;
std::vector<std::array<float, 3>> g_spins;
std::array<float, 3> g_transition_from = {0.0F, 0.0F, 0.0F};
std::array<float, 3> g_camera_from = {0.0F, 0.0F, 0.0F};
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
    const Preset::OptionState& option = g_eff().options.front();
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

void ApplyChoiceCamera() {
    if (g_eff().options.empty() || g_choices.empty()) return;
    const Preset::OptionState& option = g_eff().options.front();
    const int index = std::clamp(g_choices.front(), 0, (int)option.choices.size() - 1);
    const Preset::ChoiceState& choice = option.choices[(size_t)index];
    if (!choice.moves_camera) return;

    std::array<float, 3> eye = choice.camera_eye;
    if (option.transition_frames > 0 && g_transition > 0) {
        const float t = 1.0F - ((float)g_transition / (float)option.transition_frames);
        for (size_t i = 0; i < eye.size(); i++)
            eye[i] = g_camera_from[i] + ((eye[i] - g_camera_from[i]) * t);
    }
    Scene3dHost::SetView(eye, g_eff().camera.at, g_eff().camera.up);
}

bool Moves(const Preset::ModelMotion& motion) {
    return motion.orbit_radius > 0.0F || motion.spin_kick > 0.0F ||
           motion.spin_per_frame != std::array<float, 3>{0.0F, 0.0F, 0.0F};
}

void ApplyMotion() {
    if (g_eff().models.empty()) return;
    const bool chooses = !g_eff().options.empty() && !g_choices.empty();
    if (g_transition > 0 && !g_eff().options.empty())
        g_transition -= std::max(1, g_eff().options.front().transition_step);
    ApplyChoiceCamera();

    for (size_t i = 0; i < g_eff().models.size() && i < g_spins.size(); i++) {
        const Preset::ModelState& layer = g_eff().models[i];
        const Preset::ModelMotion& motion = layer.motion;
        const bool lead = (i == 0);
        if (!Moves(motion) && !(lead && chooses)) continue;

        if (lead) {
            if (g_spin_kick > 1.0F) {
                g_spin_kick = std::max(1.0F, g_spin_kick - motion.spin_kick_decay);
            } else if (g_spin_kick < -1.0F) {
                g_spin_kick = std::min(-1.0F, g_spin_kick + motion.spin_kick_decay);
            }
        }
        const float kick = lead ? g_spin_kick : 1.0F;
        std::array<float, 3>& spin = g_spins[i];
        for (size_t axis = 0; axis < spin.size(); axis++)
            spin[axis] += motion.spin_per_frame[axis] * kick;

        std::array<float, 3> position = layer.position;
        if (motion.orbit_radius > 0.0F) {
            position = OrbitPosition(motion);
        } else if (lead && chooses) {
            position = ChoicePosition();
        }
        Scene3dHost::SetModelTransform(std::string(layer.model), position,
                                       {layer.rotation[0] + spin[0], layer.rotation[1] + spin[1],
                                        layer.rotation[2] + spin[2]});
    }
}

void ApplyIntro() {
    const Preset::Intro& intro = g_eff().intro;
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

    const Preset::Countdown& cd = g_eff().countdown;
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

void PlaceSprites(const Preset::Effective& scene) {
    std::vector<const Preset::SpriteState*> ordered;
    ordered.reserve(scene.sprites.size());
    for (const auto& sprite : scene.sprites) {
        if (!sprite.visible) continue;
        ordered.push_back(&sprite);
    }
    std::ranges::stable_sort(ordered,
                             [](const Preset::SpriteState* a, const Preset::SpriteState* b) {
                                 return a->priority > b->priority;
                             });
    std::vector<Gc2dHost::SpritePlacement> placements;
    placements.reserve(ordered.size());
    for (const auto* sprite : ordered) {
        placements.push_back(Gc2dHost::SpritePlacement{.name = sprite->sprite,
                                                       .animated = sprite->animated,
                                                       .priority = sprite->priority,
                                                       .x = sprite->x,
                                                       .y = sprite->y,
                                                       .alpha = sprite->alpha,
                                                       .blend = (GcAnim::Blend)sprite->blend,
                                                       .timing = sprite->timing,
                                                       .skip_parts = sprite->hidden_parts,
                                                       .scroll_x = sprite->scroll_x,
                                                       .scroll_wrap = sprite->scroll_wrap});
    }
    Gc2dHost::SetSprites(std::move(placements));
}

void PushRebind() {
    const Preset::Effective& scene = g_eff();
    Scene3dHost::SetStyle((scene.shading == Preset::Shading::LitMaterial)
                              ? Scene3d::RenderStyle::LitMaterial
                              : Scene3d::RenderStyle::TextureOnly);
    Scene3dHost::SetView(scene.camera.eye, scene.camera.at, scene.camera.up);
    Scene3dHost::SetProjection(
        Scene3d::Projection{.fov_y = scene.camera.fov_y,
                            .near_z = scene.camera.near_z,
                            .far_z = scene.camera.far_z,
                            .aspect = scene.aspect_auto ? 0.0F : scene.aspect_value});
    std::vector<Scene3d::Light> lights;
    lights.reserve(scene.lights.size());
    for (const auto& light : scene.lights) {
        lights.push_back(Scene3d::Light{
            .direction = light.direction, .diffuse = light.diffuse, .specular = light.specular});
    }
    Scene3dHost::SetLights(lights);
    for (const auto& layer : scene.models) {
        Scene3dHost::SetModelAlpha(layer.model, layer.alpha);
        Scene3dHost::SetModelSpeed(layer.model, layer.anim_speed);
        Scene3dHost::SetModelBlendByName(layer.model, layer.blend_mode);
        Scene3dHost::SetModelScale(layer.model, layer.scale);
        Scene3dHost::SetModelVisibleByName(layer.model, layer.visible);
    }
}

void ResetPlayback(const Preset::Effective& scene) {
    g_countdown = scene.countdown.start_frames;
    g_frame = 0;
    g_transition = 0;
    g_spins.assign(scene.models.size(), std::array<float, 3>{0.0F, 0.0F, 0.0F});
    g_spin_kick =
        scene.models.empty() ? 1.0F : std::max(1.0F, scene.models.front().motion.spin_kick);
    g_choices.clear();
    for (const auto& option : scene.options)
        g_choices.push_back(option.default_choice);
    g_transition_from = g_choices.empty() ? std::array<float, 3>{0.0F, 0.0F, 0.0F}
                                          : scene.options.front().choices[0].position;
    g_camera_from = g_choices.empty() ? std::array<float, 3>{0.0F, 0.0F, 0.0F}
                                      : scene.options.front().choices[0].camera_eye;
    g_speed = scene.models.empty() ? 1.0F : scene.models.front().anim_speed;
    g_alpha = scene.models.empty() ? 1.0F : scene.models.front().alpha;
    g_blend = scene.models.empty() ? 0 : scene.models.front().blend_mode;
}

Scene3dHost::Setup BuildSetup(const Preset::Effective& scene, std::string_view scene_dir) {
    Scene3dHost::Setup setup;
    setup.style = (scene.shading == Preset::Shading::LitMaterial)
                      ? Scene3d::RenderStyle::LitMaterial
                      : Scene3d::RenderStyle::TextureOnly;
    setup.projection.fov_y = scene.camera.fov_y;
    setup.projection.near_z = scene.camera.near_z;
    setup.projection.far_z = scene.camera.far_z;
    setup.projection.aspect = scene.aspect_auto ? 0.0F : scene.aspect_value;
    setup.eye = scene.camera.eye;
    setup.at = scene.camera.at;
    setup.up = scene.camera.up;
    for (const auto& light : scene.lights) {
        setup.lights.push_back(Scene3d::Light{
            .direction = light.direction, .diffuse = light.diffuse, .specular = light.specular});
    }
    for (const auto& layer : scene.models) {
        if (layer.scene_dir != scene_dir) continue;
        setup.models.push_back(Scene3dHost::ModelSetup{.model = layer.model,
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
    g_mat = Preset::Materialize(scene, g_tweaks);
    const Preset::Effective& eff = g_eff();

    if (!eff.models.empty()) {
        const std::string& dir = eff.models.front().scene_dir;
        const std::string full = Resolve(game_dir, dir);
        if (!Scene3dHost::LoadWithSetup(full, BuildSetup(eff, dir))) {
            LOG("Preset", "3D layer '%s' failed to load", full.c_str());
            return false;
        }
        g_lead_model = eff.models.front().model;
    }

    if (!eff.sprites.empty()) {
        const std::string full = Resolve(game_dir, eff.sprites.front().package_dir);
        if (!Gc2dHost::Load(full)) {
            LOG("Preset", "2D layer '%s' failed to load", full.c_str());
            Scene3dHost::Unload();
            return false;
        }
        PlaceSprites(eff);
    }

    g_scene = &scene;
    g_game_dir = game_dir;
    ResetPlayback(eff);
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
    const int split = g_eff().sprite_split_priority;
    Gc2dHost::DrawSprites(split, INT_MAX);
    Scene3dHost::RenderFrame(dt);
    Gc2dHost::DrawSprites(INT_MIN, split - 1);
    Gc2dHost::AdvanceSprites(dt);
    Advance();
}

int NaturalFrames() {
    if (g_scene == nullptr) return 0;
    if (g_eff().countdown.start_frames > 0) return g_eff().countdown.start_frames;
    if (g_eff().models.empty()) return 0;
    const float speed = g_eff().models.front().anim_speed;
    const float ticks = Scene3dHost::GetStatus().max_time;
    if (speed <= 0.0F || ticks <= 0.0F) return 0;
    return (int)std::lround(ticks / speed);
}

void Restart() {
    if (g_scene == nullptr) return;
    g_countdown = g_eff().countdown.start_frames;
    g_frame = 0;
    Scene3dHost::SetTime(0.0F);
    Gc2dHost::SetFrame(0);
    if (g_eff().models.empty()) return;
    const Preset::ModelState& lead = g_eff().models.front();
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
    s.id = g_eff().id;
    s.name = g_eff().name;
    s.countdown = g_countdown;
    s.countdown_start = g_eff().countdown.start_frames;
    s.model_speed = g_speed;
    s.model_alpha = g_alpha;
    s.blend_mode = g_blend;
    s.option_choices = g_choices;
    return s;
}

void SetOption(int option, int choice) {
    if (g_scene == nullptr) return;
    if (option < 0 || (size_t)option >= g_choices.size()) return;
    const Preset::OptionState& spec = g_eff().options[(size_t)option];
    const int clamped = std::clamp(choice, 0, (int)spec.choices.size() - 1);
    if (clamped == g_choices[(size_t)option]) return;

    const int previous = g_choices[(size_t)option];
    g_transition_from = ChoicePosition();
    const auto& choices = spec.choices;
    const int current = std::clamp(g_choices[(size_t)option], 0, (int)choices.size() - 1);
    g_camera_from = choices[(size_t)current].camera_eye;
    g_choices[(size_t)option] = clamped;
    g_transition = spec.transition_frames;
    const bool forward = clamped > previous;
    g_spin_kick = std::max(1.0F, spec.spin_kick);
    if (!forward) g_spin_kick = -g_spin_kick;
}

void SetCountdown(int frames) {
    if (g_scene == nullptr) return;
    g_countdown = std::clamp(frames, 0, g_eff().countdown.start_frames);
    if (g_eff().models.empty()) return;
    if (g_countdown < g_eff().countdown.ramp_below) return;
    const Preset::ModelState& lead = g_eff().models.front();
    g_speed = lead.anim_speed;
    g_alpha = lead.alpha;
    g_blend = lead.blend_mode;
    Scene3dHost::SetModelSpeed(g_lead_model, g_speed);
    Scene3dHost::SetModelAlpha(g_lead_model, g_alpha);
    Scene3dHost::SetModelBlendByName(g_lead_model, g_blend);
}

namespace {

const Preset::ParamInstance* FindParam(const std::string& id) {
    for (const auto& param : g_mat.params) {
        if (param.id == id) return &param;
    }
    return nullptr;
}

bool TouchesSprites(const Preset::ParamInstance& param) {
    return param.target.scope == Preset::Scope::Sprite ||
           param.target.scope == Preset::Scope::SpriteTiming;
}

void Rematerialize(bool rebind, bool replace_sprites) {
    if (g_scene == nullptr) return;
    const int countdown = g_countdown;
    const std::vector<int> choices = g_choices;
    g_mat = Preset::Materialize(*g_scene, g_tweaks);
    if (rebind) PushRebind();
    if (replace_sprites) PlaceSprites(g_eff());
    g_choices = choices;
    g_choices.resize(g_eff().options.size(), 0);
    SetCountdown(countdown);
}

void RematerializeAll() {
    Rematerialize(true, true);
}

}

std::vector<StateView> ListStates() {
    std::vector<StateView> out;
    if (g_scene == nullptr) return out;
    for (size_t i = 0; i < g_eff().options.size(); i++) {
        const Preset::OptionState& option = g_eff().options[i];
        StateView view;
        view.id = option.id;
        view.label = option.label;
        view.choice = (i < g_choices.size()) ? g_choices[i] : option.default_choice;
        for (const auto& choice : option.choices) {
            view.choices.push_back(choice.label);
            if (choice.moves_camera) view.moves_camera = true;
        }
        out.push_back(std::move(view));
    }
    return out;
}

std::vector<ParamView> ListParams() {
    std::vector<ParamView> out;
    if (g_scene == nullptr) return out;
    out.reserve(g_mat.params.size());
    for (const auto& param : g_mat.params) {
        const Preset::Value value = Preset::ReadParam(param, g_eff());
        ParamView view;
        view.id = param.id;
        view.label = param.label;
        view.group = std::string(param.desc->group);
        view.unit = std::string(param.desc->unit);
        view.help = std::string(param.desc->help);
        view.aliases = std::string(param.desc->aliases);
        view.kind = (int)param.desc->kind;
        view.min = param.desc->range.min;
        view.max = param.desc->range.max;
        view.step = param.desc->range.step;
        view.soft = param.desc->range.soft;
        view.value = value.f;
        view.ivalue = value.i;
        view.fallback = param.fallback.f;
        view.ifallback = param.fallback.i;
        view.overridden = !Preset::SameValue(value, param.fallback);
        for (const std::string_view label : param.desc->enum_labels)
            view.enum_labels.emplace_back(label);
        out.push_back(std::move(view));
    }
    return out;
}

void SetParam(const std::string& id, const std::array<float, 3>& value, int ivalue) {
    const Preset::ParamInstance* param = FindParam(id);
    if (param == nullptr) return;
    Preset::Value next;
    next.kind = param->desc->kind;
    next.f = value;
    next.i = ivalue;
    Preset::SetTweak(g_tweaks, id, Preset::ClampValue(*param->desc, next));
    Rematerialize(param->desc->apply == Preset::Apply::Rebind, TouchesSprites(*param));
}

void ResetParam(const std::string& id) {
    const Preset::ParamInstance* param = FindParam(id);
    const bool rebind = (param == nullptr) || param->desc->apply == Preset::Apply::Rebind;
    const bool sprites = (param == nullptr) || TouchesSprites(*param);
    Preset::ClearTweak(g_tweaks, id);
    Rematerialize(rebind, sprites);
}

void ResetGroup(const std::string& group) {
    for (const auto& param : g_mat.params) {
        if (param.desc->group != group) continue;
        Preset::ClearTweak(g_tweaks, param.id);
    }
    RematerializeAll();
}

void ResetAllParams() {
    g_tweaks.clear();
    RematerializeAll();
}

int LoadTweaks(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        LOG("Preset", "tweak file '%s' could not be opened", path.c_str());
        return 0;
    }
    int applied = 0;
    std::string line;
    while (std::getline(file, line)) {
        const size_t eq = line.find('=');
        if (line.empty() || line[0] == '#' || eq == std::string::npos) continue;
        std::string id = line.substr(0, eq);
        while (!id.empty() && id.back() == ' ')
            id.pop_back();
        std::istringstream values(line.substr(eq + 1));
        Preset::Value value;
        values >> value.f[0] >> value.f[1] >> value.f[2] >> value.i;
        Preset::SetTweak(g_tweaks, id, value);
        applied++;
    }
    LOG("Preset", "tweak file '%s': %d override(s)", path.c_str(), applied);
    RematerializeAll();
    return applied;
}

int ChangedParamCount() {
    if (g_scene == nullptr) return 0;
    int changed = 0;
    for (const auto& param : g_mat.params) {
        if (!Preset::SameValue(Preset::ReadParam(param, g_eff()), param.fallback)) changed++;
    }
    return changed;
}

}
