#include "preset/eval/eval_resolve.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/eval_camera_lights.h"
#include "preset/eval/eval_models.h"
#include "preset/eval/eval_scene.h"
#include "preset/eval/eval_sprites.h"
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
            slot.blend_mode = (int)draw->blend_mode;
            slot.alpha = (float)draw->alpha;
            slot.anim_speed = (float)draw->anim_speed;
            slot.scale = {(float)draw->scale[0], (float)draw->scale[1], (float)draw->scale[2]};
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

void ApplySceneClip(const Doc::Clip& clip, FrameState& state) {
    switch (Doc::TypeOf(clip.command)) {
    case Doc::CommandType::RenderSettings: {
        const auto& command = std::get<Doc::RenderSettingsCmd>(clip.command);
        if (command.shading.has_value()) state.shading = *command.shading;
        if (command.sprite_split_priority.has_value())
            state.sprite_split_priority = *command.sprite_split_priority;
        break;
    }
    case Doc::CommandType::RhythmBeat: {
        const auto& command = std::get<Doc::RhythmBeat>(clip.command);
        state.beat = BeatState{.rate = command.rate,
                               .span = command.span,
                               .offset_a = command.offset_a,
                               .offset_b = command.offset_b};
        break;
    }
    case Doc::CommandType::RhythmJitter: {
        const auto& command = std::get<Doc::RhythmJitter>(clip.command);
        state.jitter = JitterState{.active = command.span > 0,
                                   .span = command.span,
                                   .scale = (float)command.scale,
                                   .mode = command.mode,
                                   .models = command.models};
        break;
    }
    case Doc::CommandType::ParamOverride: {
        const auto& command = std::get<Doc::ParamOverrideCmd>(clip.command);
        WriteTarget(command.id, OverrideToTween(command.value), state);
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
        if (Doc::TypeOf(clip.command) == Doc::CommandType::CameraSet) {
            ApplyCameraSet(clip, std::get<Doc::CameraSet>(clip.command), frame, state.camera,
                           state.writes, tweens);
        } else if (tweens) {
            ApplyCameraTween(clip, frame, state.camera, state.writes);
        }
        break;
    case Doc::TrackKind::Light:
        ApplyLightSet(std::get<Doc::LightSet>(clip.command), state.lights);
        break;
    case Doc::TrackKind::Fx:
        state.emitters.push_back(&clip);
        break;
    case Doc::TrackKind::Scene:
        ApplySceneClip(clip, state);
        break;
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
    state.camera = CameraFrom(document.camera, document.render.width, document.render.height);
    for (const Doc::LightSpec& light : document.lights) {
        LightState entry;
        for (std::size_t i = 0; i < entry.direction.size(); i++) {
            entry.direction[i] = (float)light.direction[i];
            entry.diffuse[i] = (float)light.diffuse[i];
            entry.specular[i] = (float)light.specular[i];
        }
        state.lights.push_back(entry);
    }

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
    return state;
}

}
