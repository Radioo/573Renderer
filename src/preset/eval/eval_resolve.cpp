#include "preset/eval/eval_resolve.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/eval_camera_lights.h"
#include "preset/eval/eval_models.h"
#include "preset/eval/eval_poly.h"
#include "preset/eval/eval_scene.h"
#include "preset/eval/eval_sprites.h"
#include "preset/eval/eval_tween.h"
#include "preset/eval/frame_state.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Preset::Eval {

namespace {

struct Slots {
    std::vector<std::string> models;
    std::vector<std::string> sprites;
};

bool ClipActive(const Doc::Clip& clip, int frame) {
    if (Doc::IsEvent(Doc::TypeOf(clip.command))) return frame == clip.start;
    if (frame < clip.start) return false;
    return !clip.end.has_value() || frame < *clip.end;
}

int IndexOfName(const std::vector<std::string>& names, const std::string& name) {
    for (std::size_t i = 0; i < names.size(); i++) {
        if (names[i] == name) return (int)i;
    }
    return -1;
}

Slots CollectTargets(const Doc::Document& document) {
    Slots out;
    for (const Doc::Track& track : document.tracks) {
        if (track.kind == Doc::TrackKind::Model && IndexOfName(out.models, track.target) < 0)
            out.models.push_back(track.target);
        if (track.kind == Doc::TrackKind::Sprite && IndexOfName(out.sprites, track.target) < 0)
            out.sprites.push_back(track.target);
    }
    return out;
}

void SeedHiddenMaterial(const Doc::Document& document, const Slots& slots, FrameState& state) {
    for (const Doc::Track& track : document.tracks) {
        if (track.kind != Doc::TrackKind::Model) continue;
        const int index = IndexOfName(slots.models, track.target);
        if (index < 0) continue;
        ModelSlot& slot = state.models[(std::size_t)index];
        for (const Doc::Clip& clip : track.clips) {
            const auto* draw = std::get_if<Doc::ModelDraw>(&clip.command);
            if (draw == nullptr) continue;
            if (slot.draw_start >= 0 && clip.start >= slot.draw_start) continue;
            slot.draw_start = clip.start;
            slot.asset = draw->asset;
            slot.mesh = draw->model.empty() ? slot.name : draw->model;
            slot.blend_mode = (int)draw->blend_mode;
            slot.alpha = (float)draw->alpha;
            slot.anim_speed = (float)draw->anim_speed;
            slot.scale = {(float)draw->scale[0], (float)draw->scale[1], (float)draw->scale[2]};
            slot.from.blend_mode = &clip;
            slot.from.alpha = &clip;
            slot.from.anim_speed = &clip;
            slot.from.scale = &clip;
        }
        slot.draw_start = -1;
    }
}

bool Abuts(const Doc::Track& track, const Doc::Clip& clip) {
    return std::ranges::any_of(track.clips, [&clip](const Doc::Clip& other) {
        return other.end.has_value() && *other.end == clip.start;
    });
}

void ApplyModelClip(const Doc::Clip& clip, int frame, int index, bool tweens, FrameState& state) {
    if (index < 0) return;
    ModelSlot& slot = state.models[(std::size_t)index];
    switch (Doc::TypeOf(clip.command)) {
    case Doc::CommandType::ModelDraw:
        ApplyModelDraw(clip, std::get<Doc::ModelDraw>(clip.command), frame, slot, index,
                       state.writes, tweens);
        break;
    case Doc::CommandType::ModelTween:
        if (tweens) ApplyModelTween(clip, frame, slot, index, state.writes);
        break;
    case Doc::CommandType::ModelMotion:
        ApplyModelMotion(clip, std::get<Doc::ModelMotionCmd>(clip.command), slot);
        break;
    case Doc::CommandType::ModelEase:
        slot.from.ease = &clip;
        break;
    default:
        break;
    }
}

void ApplySpriteClip(const Doc::Track& track, const Doc::Clip& clip, int frame, int index,
                     bool tweens, FrameState& state) {
    if (index < 0) return;
    SpriteSlot& slot = state.sprites[(std::size_t)index];
    switch (Doc::TypeOf(clip.command)) {
    case Doc::CommandType::SpriteDraw:
        ApplySpriteDraw(clip, std::get<Doc::SpriteDraw>(clip.command), frame, slot, tweens);
        break;
    case Doc::CommandType::SpriteAnimate:
        ApplySpriteAnimate(clip, std::get<Doc::SpriteAnimate>(clip.command), frame, slot,
                           Abuts(track, clip), tweens);
        break;
    case Doc::CommandType::SpriteScroll:
        ApplySpriteScroll(clip, std::get<Doc::SpriteScroll>(clip.command), frame, slot, tweens);
        break;
    default:
        break;
    }
}

void ApplyRenderSettings(const Doc::Clip& clip, int frame, bool tweens, FrameState& state) {
    const auto& command = std::get<Doc::RenderSettingsCmd>(clip.command);
    if (command.shading.has_value()) {
        state.shading = *command.shading;
        state.shading_from = &clip;
    }
    if (command.sprite_split_priority.has_value()) {
        state.sprite_split_priority = *command.sprite_split_priority;
        state.split_from = &clip;
    }
    if (command.clear_color.has_value()) {
        state.clear_color = {(float)(*command.clear_color)[0], (float)(*command.clear_color)[1],
                             (float)(*command.clear_color)[2]};
        state.clear_from = &clip;
    }
    if (!tweens) return;
    TweenValue sampled;
    TweenValue underlying;
    underlying.kind = TweenValue::Kind::Vector;
    underlying.vector = state.clear_color;
    if (SampleKeys(clip.keys, "clear_color", frame - clip.start, underlying, sampled)) {
        state.clear_color = sampled.vector;
        state.clear_from = &clip;
    }
}

void ApplyFog(const Doc::Clip& clip, int frame, bool tweens, FrameState& state) {
    const auto& command = std::get<Doc::FogCmd>(clip.command);
    state.fog.from = &clip;
    state.fog.enabled = command.enabled;
    state.fog.color = {(float)command.color[0], (float)command.color[1], (float)command.color[2]};
    state.fog.start = (float)command.start;
    state.fog.end = (float)command.end;
    state.fog.density = (float)command.density;
    if (!tweens) return;
    for (const std::string_view field : {"color", "start", "end", "density"}) {
        const std::string id = "fog." + std::string(field);
        TweenValue underlying;
        TweenValue sampled;
        if (!ReadTarget(id, state, underlying)) continue;
        if (!SampleKeys(clip.keys, field, frame - clip.start, underlying, sampled)) continue;
        WriteTarget(id, sampled, state, &clip);
    }
}

void ApplySceneClip(const Doc::Clip& clip, int frame, bool tweens, FrameState& state) {
    switch (Doc::TypeOf(clip.command)) {
    case Doc::CommandType::RenderSettings:
        ApplyRenderSettings(clip, frame, tweens, state);
        break;
    case Doc::CommandType::Fog:
        ApplyFog(clip, frame, tweens, state);
        break;
    case Doc::CommandType::ClearCycle:
        state.clear_cycle = &clip;
        break;
    case Doc::CommandType::RhythmBeat: {
        const auto& command = std::get<Doc::RhythmBeat>(clip.command);
        state.beat = BeatState{.from = &clip,
                               .rate = command.rate,
                               .span = command.span,
                               .offset_a = command.offset_a,
                               .offset_b = command.offset_b};
        break;
    }
    case Doc::CommandType::RhythmJitter: {
        const auto& command = std::get<Doc::RhythmJitter>(clip.command);
        state.jitter = JitterState{.from = &clip,
                                   .active = command.span > 0,
                                   .span = command.span,
                                   .scale = (float)command.scale,
                                   .mode = command.mode,
                                   .models = command.models};
        break;
    }
    case Doc::CommandType::ParamOverride: {
        const auto& command = std::get<Doc::ParamOverrideCmd>(clip.command);
        WriteTarget(command.id, OverrideToTween(command.value), state, &clip);
        break;
    }
    case Doc::CommandType::RngSeed:
        state.seeds.push_back(&clip);
        break;
    case Doc::CommandType::OptionSelect:
        state.selects.push_back(&clip);
        break;
    default:
        break;
    }
}

void ApplyClip(const Doc::Track& track, const Doc::Clip& clip, int frame, bool tweens,
               const Slots& slots, FrameState& state) {
    switch (track.kind) {
    case Doc::TrackKind::Model:
        ApplyModelClip(clip, frame, IndexOfName(slots.models, track.target), tweens, state);
        break;
    case Doc::TrackKind::Sprite:
        ApplySpriteClip(track, clip, frame, IndexOfName(slots.sprites, track.target), tweens,
                        state);
        break;
    case Doc::TrackKind::Camera:
        switch (Doc::TypeOf(clip.command)) {
        case Doc::CommandType::CameraSet:
            ApplyCameraSet(clip, std::get<Doc::CameraSet>(clip.command), frame, state.camera,
                           state.writes, tweens);
            break;
        case Doc::CommandType::CameraEase:
            state.camera.from.ease = &clip;
            break;
        case Doc::CommandType::CameraMotion:
            state.camera.from.motion = &clip;
            break;
        default:
            if (tweens) ApplyCameraTween(clip, frame, state.camera, state.writes);
            break;
        }
        break;
    case Doc::TrackKind::Light:
        ApplyLightSet(clip, std::get<Doc::LightSet>(clip.command), state.lights);
        break;
    case Doc::TrackKind::Fx:
        state.emitters.push_back(&clip);
        break;
    case Doc::TrackKind::Scene:
        ApplySceneClip(clip, frame, tweens, state);
        break;
    case Doc::TrackKind::Poly:
        ApplyPolyGrid(clip, std::get<Doc::PolyTileGrid>(clip.command), frame, state.poly);
        break;
    }
}

void SeedLights(const Doc::Document& document, FrameState& state) {
    for (const Doc::LightSpec& light : document.lights) {
        LightState entry;
        for (std::size_t i = 0; i < entry.direction.size(); i++) {
            entry.direction[i] = (float)light.direction[i];
            entry.diffuse[i] = (float)light.diffuse[i];
            entry.specular[i] = (float)light.specular[i];
            entry.ambient[i] = (float)light.ambient[i];
        }
        state.lights.push_back(entry);
    }
}

bool GateMatches(const Doc::Gate& gate, const Doc::Document& document,
                 const std::vector<int>& choices) {
    for (std::size_t i = 0; i < document.options.size(); i++) {
        const Doc::OptionSpec& option = document.options[i];
        if (option.id != gate.option) continue;
        const int index = SelectedChoice(option, choices, i);
        if (index < 0) return false;
        const std::string& label = option.choices[(std::size_t)index].label;
        const bool listed = std::ranges::find(gate.choices, label) != gate.choices.end();
        return gate.kind == Doc::GateKind::Not ? !listed : listed;
    }
    return false;
}

}

int SelectedChoice(const Doc::OptionSpec& option, const std::vector<int>& choices,
                   std::size_t index) {
    if (option.choices.empty() || index >= choices.size()) return -1;
    return std::clamp(choices[index], 0, (int)option.choices.size() - 1);
}

FrameState ResolveFrame(const ResolveInput& input, int frame) {
    FrameState state;
    if (input.document == nullptr || input.choices == nullptr) return state;
    const Doc::Document& document = *input.document;
    state.frame = frame;
    state.shading = document.render.shading;
    state.sprite_split_priority = document.render.sprite_split_priority;
    for (std::size_t i = 0; i < state.clear_color.size(); i++)
        state.clear_color[i] = (float)document.render.clear_color[i];
    state.camera = CameraFrom(document.camera, document.render.width, document.render.height);
    SeedLights(document, state);

    const Slots slots = CollectTargets(document);
    for (const std::string& name : slots.models) {
        ModelSlot slot;
        slot.name = name;
        state.models.push_back(std::move(slot));
    }
    for (const std::string& name : slots.sprites) {
        SpriteSlot slot;
        slot.name = name;
        state.sprites.push_back(std::move(slot));
    }
    SeedHiddenMaterial(document, slots, state);

    const bool any_solo =
        std::ranges::any_of(document.tracks, [](const Doc::Track& track) { return track.solo; });
    for (const Doc::Track& track : document.tracks) {
        if (track.muted || (any_solo && !track.solo)) continue;
        for (const Doc::Clip& clip : track.clips) {
            if (clip.muted || !ClipActive(clip, frame)) continue;
            if (clip.when.has_value() && !GateMatches(*clip.when, document, *input.choices))
                continue;
            ApplyClip(track, clip, frame, input.tweens, slots, state);
        }
    }
    if (state.poly.active && document.fps > 0)
        state.poly.seconds = (float)frame / (float)document.fps;
    if (state.clear_cycle != nullptr) {
        state.clear_color =
            ClearCycleColor(std::get<Doc::ClearCycleCmd>(state.clear_cycle->command), frame);
        state.clear_from = state.clear_cycle;
    }
    return state;
}

}
