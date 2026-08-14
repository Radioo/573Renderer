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

std::string Slot(std::string_view prefix, const std::string& name, size_t index,
                 size_t occurrence) {
    std::string slot(prefix);
    slot += '[';
    slot += name.empty() ? std::to_string(index) : name;
    if (occurrence > 0) {
        slot += '#';
        slot += std::to_string(occurrence + 1);
    }
    slot += ']';
    return slot;
}

std::string Bracket(const std::string& slot, std::string_view key) {
    std::string id(slot);
    if (!key.empty()) {
        id += '.';
        id += key;
    }
    return id;
}

size_t Occurrence(const std::vector<std::string>& names, size_t index) {
    size_t seen = 0;
    for (size_t i = 0; i < index && i < names.size(); i++) {
        if (names[i] == names[index]) seen++;
    }
    return seen;
}

std::vector<std::string> ModelNames(const Effective& src) {
    std::vector<std::string> names;
    names.reserve(src.models.size());
    for (const auto& model : src.models)
        names.push_back(model.model);
    return names;
}

std::vector<std::string> SpriteNames(const Effective& src) {
    std::vector<std::string> names;
    names.reserve(src.sprites.size());
    for (const auto& sprite : src.sprites)
        names.push_back(sprite.sprite);
    return names;
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
    out.beat = src.beat;
    out.pulse = src.pulse;
    out.jitter = src.jitter;
    out.rng_seed = src.rng_seed;
    out.opaque_screen = src.opaque_screen;
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
        state.scale = layer.scale;
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
                 std::string label, std::string group, const Target& target,
                 const Effective& pristine) {
    ParamInstance instance;
    instance.id = std::move(id);
    instance.label = std::move(label);
    instance.group = std::move(group);
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
    case Scope::Beat:
        return &out.beat;
    case Scope::Pulse:
        return &out.pulse;
    case Scope::Jitter:
        return &out.jitter;
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

namespace {

void InstantiateModels(const ParamDesc& desc, const Effective& pristine,
                       std::vector<ParamInstance>& out) {
    const std::vector<std::string> names = ModelNames(pristine);
    for (size_t i = 0; i < names.size(); i++) {
        const std::string slot = Slot("model", names[i], i, Occurrence(names, i));
        AddInstance(out, desc, Bracket(slot, desc.key), std::string(desc.label),
                    "Model " + slot.substr(6, slot.size() - 7),
                    Target{.scope = desc.scope, .index = (int16_t)i}, pristine);
    }
}

void InstantiateSprites(const ParamDesc& desc, const Effective& pristine,
                        std::vector<ParamInstance>& out) {
    const std::vector<std::string> names = SpriteNames(pristine);
    for (size_t i = 0; i < names.size(); i++) {
        const std::string slot = Slot("sprite", names[i], i, Occurrence(names, i));
        AddInstance(out, desc, Bracket(slot, desc.key), std::string(desc.label),
                    "2D layer " + slot.substr(7, slot.size() - 8),
                    Target{.scope = desc.scope, .index = (int16_t)i}, pristine);
    }
}

void InstantiateOptionChoices(const ParamDesc& desc, const Effective& pristine,
                              std::vector<ParamInstance>& out) {
    for (size_t i = 0; i < pristine.options.size(); i++) {
        const OptionState& option = pristine.options[i];
        std::vector<std::string> labels;
        labels.reserve(option.choices.size());
        for (const auto& choice : option.choices)
            labels.push_back(choice.label);
        for (size_t c = 0; c < labels.size(); c++) {
            std::string id = Slot("option", option.id, i, 0);
            id += '.';
            id += Slot("choice", labels[c], c, Occurrence(labels, c));
            id += '.';
            id += desc.key;
            AddInstance(out, desc, std::move(id), labels[c], "States: " + option.label,
                        Target{.scope = desc.scope, .index = (int16_t)i, .sub = (int16_t)c},
                        pristine);
        }
    }
}

}

std::vector<ParamInstance> Instantiate(const Effective& pristine) {
    std::vector<ParamInstance> out;
    for (const ParamDesc& desc : Schema()) {
        switch (desc.scope) {
        case Scope::Scene:
        case Scope::Camera:
        case Scope::Countdown:
        case Scope::Intro:
        case Scope::Beat:
        case Scope::Pulse:
        case Scope::Jitter:
            AddInstance(out, desc, std::string(desc.key), std::string(desc.label),
                        std::string(desc.group), Target{.scope = desc.scope}, pristine);
            break;
        case Scope::Light:
            for (size_t i = 0; i < pristine.lights.size(); i++) {
                const std::string slot = Slot("light", "", i, 0);
                AddInstance(out, desc, Bracket(slot, desc.key), std::string(desc.label),
                            "Lighting " + std::to_string(i),
                            Target{.scope = desc.scope, .index = (int16_t)i}, pristine);
            }
            break;
        case Scope::Model:
        case Scope::ModelMotion:
            InstantiateModels(desc, pristine, out);
            break;
        case Scope::Sprite:
        case Scope::SpriteTiming:
            InstantiateSprites(desc, pristine, out);
            break;
        case Scope::Option:
            for (size_t i = 0; i < pristine.options.size(); i++) {
                const std::string slot = Slot("option", pristine.options[i].id, i, 0);
                AddInstance(out, desc, Bracket(slot, desc.key), std::string(desc.label),
                            "States: " + pristine.options[i].label,
                            Target{.scope = desc.scope, .index = (int16_t)i}, pristine);
            }
            break;
        case Scope::OptionChoice:
            InstantiateOptionChoices(desc, pristine, out);
            break;
        }
    }
    return out;
}

namespace {

void Write(const ParamInstance& param, Effective& out, const Value& value) {
    void* owner = ResolveOwner(out, param.target);
    if (owner == nullptr) return;
    param.desc->accessor.set(owner, ClampValue(*param.desc, value));
}

}

void WriteParam(std::span<const ParamInstance> params, const std::string& id, const Value& value,
                Effective& out) {
    for (const ParamInstance& param : params) {
        if (param.id != id) continue;
        Value next = value;
        next.kind = param.desc->kind;
        Write(param, out, next);
        return;
    }
}

Materialized Materialize(const Scene& src, std::span<const ParamOverride> phase,
                         const TweakSet& tweaks) {
    Materialized out;
    CopyScene(src, out.effective);
    CopyModels(src, out.effective);
    CopySprites(src, out.effective);
    CopyLights(src, out.effective);
    CopyOptions(src, out.effective);

    out.params = Instantiate(out.effective);
    for (const ParamOverride& override : phase) {
        for (const ParamInstance& param : out.params) {
            if (param.id != override.id) continue;
            Value value;
            value.kind = param.desc->kind;
            value.f = override.f;
            value.i = override.i;
            Write(param, out.effective, value);
        }
    }
    for (ParamInstance& param : out.params)
        param.fallback = ReadParam(param, out.effective);

    for (const ParamInstance& param : out.params) {
        const Value* tweak = FindTweak(tweaks, param.id);
        if (tweak != nullptr) Write(param, out.effective, *tweak);
    }
    return out;
}

}
