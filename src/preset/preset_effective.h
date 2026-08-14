#pragma once

#include "formats/gcanim.h"
#include "preset/preset_params.h"
#include "preset/scene_preset.h"

#include <array>
#include <span>
#include <string>
#include <vector>

namespace Preset {

struct ModelState {
    std::string scene_dir;
    std::string model;
    bool visible = true;
    int blend_mode = 0;
    float alpha = 1.0F;
    float anim_speed = 1.0F;
    std::array<float, 3> position = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> rotation = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> scale = {1.0F, 1.0F, 1.0F};
    ModelMotion motion = {};
};

struct SpriteState {
    std::string package_dir;
    std::string sprite;
    std::vector<std::string> hidden_parts;
    bool visible = true;
    bool animated = false;
    float x = 0.0F;
    float y = 0.0F;
    float alpha = 1.0F;
    float scale = 1.0F;
    int blend = 0;
    int priority = 0;
    GcAnim::Timing timing = {};
    float scroll_x = 0.0F;
    float scroll_wrap = 0.0F;
};

struct ChoiceState {
    std::string label;
    std::array<float, 3> position = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> camera_eye = {0.0F, 0.0F, 0.0F};
    bool moves_camera = false;
    std::span<const ParamOverride> params = {};
};

struct OptionState {
    std::string id;
    std::string label;
    std::vector<ChoiceState> choices;
    int default_choice = 0;
    int transition_frames = 0;
    int transition_step = 4;
    float spin_kick = 0.0F;
};

struct Effective {
    std::string id;
    std::string name;
    std::string build;
    int render_w = 640;
    int render_h = 480;
    Shading shading = Shading::TextureOnly;
    int sprite_split_priority = 30;
    Camera camera = {};
    bool aspect_auto = true;
    float aspect_value = 1.0F;
    Countdown countdown = {};
    Intro intro = {};
    Beat beat = {};
    Pulse pulse = {};
    Jitter jitter = {};
    int rng_seed = 1;
    bool opaque_screen = true;
    std::vector<ModelState> models;
    std::vector<SpriteState> sprites;
    std::vector<DirectionalLight> lights;
    std::vector<OptionState> options;
};

struct ParamInstance {
    std::string id;
    std::string label;
    std::string group;
    const ParamDesc* desc = nullptr;
    Target target;
    Value fallback;
};

struct Materialized {
    Effective effective;
    std::vector<ParamInstance> params;
};

using TweakSet = std::vector<std::pair<std::string, Value>>;

const Value* FindTweak(const TweakSet& tweaks, const std::string& id);

void SetTweak(TweakSet& tweaks, const std::string& id, const Value& value);

void ClearTweak(TweakSet& tweaks, const std::string& id);

void* ResolveOwner(Effective& out, const Target& target);

const void* ResolveOwner(const Effective& src, const Target& target);

std::vector<ParamInstance> Instantiate(const Effective& pristine);

Value ReadParam(const ParamInstance& param, const Effective& src);

void WriteParam(std::span<const ParamInstance> params, const std::string& id, const Value& value,
                Effective& out);

Materialized Materialize(const Scene& src, std::span<const ParamOverride> phase,
                         const TweakSet& tweaks);

}
