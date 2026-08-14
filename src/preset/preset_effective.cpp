#include "preset/preset_effective.h"

#include "preset/preset_params.h"
#include "preset/scene_preset.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
#include <algorithm>
#include <string>
#include <utility>

namespace Preset {

namespace {

std::string Bracket(std::string_view prefix, const std::string& name, std::string_view key) {
    std::string id(prefix);
    id += '[';
    id += name;
    id += ']';
    if (!key.empty()) {
        id += '.';
        id += key;
    }
    return id;
}

void CopyScene(const Scene& src, Effective& out) {
    out.id = std::string(src.id);
    out.name = std::string(src.name);
    out.build = std::string(src.build);
    out.render_w = src.render_w;
    out.render_h = src.render_h;
    out.shading = src.shading;
    out.sprite_split_priority = src.sprite_split_priority;
    out.camera = src.camera;
    out.aspect_auto = (src.camera.aspect == 0.0F);
    out.aspect_value =
        out.aspect_auto ? ((float)src.render_w / (float)src.render_h) : src.camera.aspect;
    out.countdown = src.countdown;
    out.intro = src.intro;
}

void CopyModels(const Scene& src, Effective& out) {
    out.models.reserve(src.models.size());
    for (const auto& layer : src.models) {
        out.models.push_back(ModelState{.scene_dir = std::string(layer.scene_dir),
                                        .model = std::string(layer.model),
                                        .blend_mode = layer.blend_mode,
                                        .alpha = layer.alpha,
                                        .anim_speed = layer.anim_speed,
                                        .position = layer.position,
                                        .rotation = layer.rotation,
                                        .scale = layer.scale,
                                        .motion = layer.motion});
    }
}

void CopySprites(const Scene& src, Effective& out) {
    out.sprites.reserve(src.sprites.size());
    for (const auto& layer : src.sprites) {
        SpriteState state;
        state.package_dir = std::string(layer.package_dir);
        state.sprite = std::string(layer.sprite);
        for (const std::string_view part : layer.hidden_parts)
            state.hidden_parts.emplace_back(part);
        state.animated = layer.animated;
        state.x = layer.x;
        state.y = layer.y;
        state.alpha = layer.alpha;
        state.blend = layer.blend;
        state.priority = layer.priority;
        state.timing = layer.timing;
        state.scroll_x = layer.scroll_x;
        state.scroll_wrap = layer.scroll_wrap;
        out.sprites.push_back(std::move(state));
    }
}

void CopyOptions(const Scene& src, Effective& out) {
    out.options.reserve(src.options.size());
    for (const auto& option : src.options) {
        OptionState state;
        state.id = std::string(option.id);
        state.label = std::string(option.label);
        state.default_choice = option.default_choice;
        state.transition_frames = option.transition_frames;
        state.spin_kick = option.spin_kick;
        for (const auto& choice : option.choices) {
            state.choices.push_back(ChoiceState{.label = std::string(choice.label),
                                                .position = choice.position,
                                                .camera_eye = choice.camera_eye,
                                                .moves_camera = choice.moves_camera});
        }
        out.options.push_back(std::move(state));
    }
}

void CopyLights(const Scene& src, Effective& out) {
    out.lights.assign(src.lights.begin(), src.lights.end());
}

void AddInstance(std::vector<ParamInstance>& out, const ParamDesc& desc, std::string id,
                 std::string label, const Target& target, const Effective& pristine) {
    ParamInstance instance;
    instance.id = std::move(id);
    instance.label = std::move(label);
    instance.desc = &desc;
    instance.target = target;
    instance.fallback = ReadParam(instance, pristine);
    out.push_back(std::move(instance));
}

}

const Value* FindTweak(const TweakSet& tweaks, const std::string& id) {
    for (const auto& [key, value] : tweaks) {
        if (key == id) return &value;
    }
    return nullptr;
}

void SetTweak(TweakSet& tweaks, const std::string& id, const Value& value) {
    for (auto& entry : tweaks) {
        if (entry.first != id) continue;
        entry.second = value;
        return;
    }
    tweaks.emplace_back(id, value);
}

void ClearTweak(TweakSet& tweaks, const std::string& id) {
    std::erase_if(tweaks, [&id](const auto& entry) { return entry.first == id; });
}

void* ResolveOwner(Effective& out, const Target& target) {
    const auto index = (size_t)std::max<int16_t>(target.index, 0);
    switch (target.scope) {
    case Scope::Scene:
        return &out;
    case Scope::Camera:
        return &out.camera;
    case Scope::Countdown:
        return &out.countdown;
    case Scope::Intro:
        return &out.intro;
    case Scope::Light:
        return (index < out.lights.size()) ? &out.lights[index] : nullptr;
    case Scope::Model:
        return (index < out.models.size()) ? &out.models[index] : nullptr;
    case Scope::ModelMotion:
        return (index < out.models.size()) ? &out.models[index].motion : nullptr;
    case Scope::Sprite:
        return (index < out.sprites.size()) ? &out.sprites[index] : nullptr;
    case Scope::SpriteTiming:
        return (index < out.sprites.size()) ? &out.sprites[index].timing : nullptr;
    case Scope::Option:
        return (index < out.options.size()) ? &out.options[index] : nullptr;
    case Scope::OptionChoice: {
        if (index >= out.options.size()) return nullptr;
        auto& choices = out.options[index].choices;
        const auto sub = (size_t)std::max<int16_t>(target.sub, 0);
        return (sub < choices.size()) ? &choices[sub] : nullptr;
    }
    }
    return nullptr;
}

const void* ResolveOwner(const Effective& src, const Target& target) {
    return ResolveOwner(const_cast<Effective&>(src), target);
}

Value ReadParam(const ParamInstance& param, const Effective& src) {
    const void* owner = ResolveOwner(src, param.target);
    if (owner == nullptr || param.desc == nullptr) return Value{};
    Value value = param.desc->accessor.get(owner);
    value.kind = param.desc->kind;
    return value;
}

std::vector<ParamInstance> Instantiate(const Effective& pristine) {
    std::vector<ParamInstance> out;
    for (const ParamDesc& desc : Schema()) {
        switch (desc.scope) {
        case Scope::Scene:
        case Scope::Camera:
        case Scope::Countdown:
        case Scope::Intro:
            AddInstance(out, desc, std::string(desc.key), std::string(desc.label),
                        Target{.scope = desc.scope}, pristine);
            break;
        case Scope::Light:
            for (size_t i = 0; i < pristine.lights.size(); i++) {
                AddInstance(out, desc, Bracket("light", std::to_string(i), desc.key),
                            std::string(desc.label),
                            Target{.scope = desc.scope, .index = (int16_t)i}, pristine);
            }
            break;
        case Scope::Model:
        case Scope::ModelMotion:
            for (size_t i = 0; i < pristine.models.size(); i++) {
                AddInstance(out, desc, Bracket("model", pristine.models[i].model, desc.key),
                            std::string(desc.label),
                            Target{.scope = desc.scope, .index = (int16_t)i}, pristine);
            }
            break;
        case Scope::Sprite:
        case Scope::SpriteTiming:
            for (size_t i = 0; i < pristine.sprites.size(); i++) {
                AddInstance(out, desc, Bracket("sprite", pristine.sprites[i].sprite, desc.key),
                            std::string(desc.label),
                            Target{.scope = desc.scope, .index = (int16_t)i}, pristine);
            }
            break;
        case Scope::Option:
            for (size_t i = 0; i < pristine.options.size(); i++) {
                AddInstance(out, desc, Bracket("option", pristine.options[i].id, desc.key),
                            std::string(desc.label),
                            Target{.scope = desc.scope, .index = (int16_t)i}, pristine);
            }
            break;
        case Scope::OptionChoice:
            for (size_t i = 0; i < pristine.options.size(); i++) {
                const auto& option = pristine.options[i];
                for (size_t c = 0; c < option.choices.size(); c++) {
                    std::string id = Bracket("option", option.id, "");
                    id += ".choice[";
                    id += option.choices[c].label;
                    id += "].";
                    id += desc.key;
                    AddInstance(out, desc, std::move(id), std::string(option.choices[c].label),
                                Target{.scope = desc.scope, .index = (int16_t)i, .sub = (int16_t)c},
                                pristine);
                }
            }
            break;
        }
    }
    return out;
}

Materialized Materialize(const Scene& src, const TweakSet& tweaks) {
    Materialized out;
    CopyScene(src, out.effective);
    CopyModels(src, out.effective);
    CopySprites(src, out.effective);
    CopyLights(src, out.effective);
    CopyOptions(src, out.effective);

    out.params = Instantiate(out.effective);
    for (const ParamInstance& param : out.params) {
        const Value* tweak = FindTweak(tweaks, param.id);
        if (tweak == nullptr) continue;
        void* owner = ResolveOwner(out.effective, param.target);
        if (owner == nullptr) continue;
        param.desc->accessor.set(owner, ClampValue(*param.desc, *tweak));
    }
    return out;
}

}
