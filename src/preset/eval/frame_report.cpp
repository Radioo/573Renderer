#include "preset/eval/frame_report.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/eval_models.h"
#include "preset/eval/eval_particles.h"
#include "preset/eval/eval_resolve.h"
#include "preset/eval/eval_state.h"
#include "preset/eval/frame_state.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Preset::Eval {

namespace {

std::string ClipId(const Doc::Clip* clip) {
    return clip == nullptr ? std::string{} : clip->id;
}

ReportValue Scalar(std::string field, float value, const Doc::Clip* from) {
    return ReportValue{.field = std::move(field),
                       .clip = ClipId(from),
                       .scalar = value,
                       .kind = ReportKind::Scalar};
}

ReportValue Vector(std::string field, const Vec3f& value, const Doc::Clip* from) {
    return ReportValue{.field = std::move(field),
                       .clip = ClipId(from),
                       .vector = value,
                       .kind = ReportKind::Vector};
}

ReportValue Integer(std::string field, int value, const Doc::Clip* from) {
    return ReportValue{.field = std::move(field),
                       .clip = ClipId(from),
                       .integer = value,
                       .kind = ReportKind::Integer};
}

ReportValue Boolean(std::string field, bool value, const Doc::Clip* from) {
    return ReportValue{.field = std::move(field),
                       .clip = ClipId(from),
                       .integer = value ? 1 : 0,
                       .kind = ReportKind::Boolean};
}

ReportValue Text(std::string field, std::string value, const Doc::Clip* from) {
    return ReportValue{.field = std::move(field),
                       .clip = ClipId(from),
                       .text = std::move(value),
                       .kind = ReportKind::Text};
}

std::string NameOf(std::span<const std::string_view> names, int index) {
    if (index < 0 || std::cmp_greater_equal(index, names.size())) return std::to_string(index);
    return std::string(names[(std::size_t)index]);
}

Vec3f SpunRotation(const ModelSlot& slot, const EvalState& runtime, std::size_t index) {
    Vec3f out = slot.rotation;
    if (index >= runtime.models.size()) return out;
    for (std::size_t axis = 0; axis < out.size(); axis++)
        out[axis] += runtime.models[index].spin[axis];
    return out;
}

void AddModels(const FrameState& state, const EvalState& runtime, FrameReport& report) {
    for (std::size_t i = 0; i < state.models.size(); i++) {
        const ModelSlot& slot = state.models[i];
        ReportEntity entity;
        entity.kind = "model";
        entity.name = slot.name;
        entity.values.push_back(Boolean("visible", slot.visible, slot.from.visible));
        entity.values.push_back(
            Vector("position", PlacedPosition(slot, state.jitter, runtime.jitter, state.frame),
                   slot.from.position));
        entity.values.push_back(
            Vector("rotation", SpunRotation(slot, runtime, i), slot.from.rotation));
        entity.values.push_back(Vector("scale", slot.scale, slot.from.scale));
        entity.values.push_back(Scalar("alpha", slot.alpha, slot.from.alpha));
        entity.values.push_back(Scalar("anim_speed", slot.anim_speed, slot.from.anim_speed));
        entity.values.push_back(Text("blend_mode", NameOf(Doc::kModelBlendNames, slot.blend_mode),
                                     slot.from.blend_mode));
        if (i < runtime.models.size())
            entity.values.push_back(Scalar("tick", runtime.models[i].tick, nullptr));
        if (slot.from.motion != nullptr)
            entity.values.push_back(Text("motion", "orbit, spin and pulse", slot.from.motion));
        report.entities.push_back(std::move(entity));
    }
}

void AddSprites(const FrameState& state, const EvalState& runtime, FrameReport& report) {
    for (std::size_t i = 0; i < state.sprites.size(); i++) {
        const SpriteSlot& slot = state.sprites[i];
        ReportEntity entity;
        entity.kind = "sprite";
        entity.name = slot.name;
        entity.values.push_back(Boolean("visible", slot.visible, slot.from.visible));
        entity.values.push_back(Text("source", slot.source, slot.from.source));
        if (i < runtime.sprite_clock.size())
            entity.values.push_back(Scalar("clock", runtime.sprite_clock[i], slot.from.source));
        entity.values.push_back(Scalar("x", slot.x, slot.from.x));
        entity.values.push_back(Scalar("y", slot.y, slot.from.y));
        entity.values.push_back(Scalar("alpha", slot.alpha, slot.from.alpha));
        entity.values.push_back(Scalar("scale", slot.scale, slot.from.scale));
        entity.values.push_back(Integer("priority", slot.priority, slot.from.priority));
        entity.values.push_back(Text(
            "side", slot.priority >= state.sprite_split_priority ? "behind the 3D" : "in front",
            state.split_from));
        if (slot.from.scroll != nullptr)
            entity.values.push_back(Scalar("scroll_x", slot.scroll_x, slot.from.scroll));
        report.entities.push_back(std::move(entity));
    }
}

void AddEmitters(const FrameState& state, const EvalState& runtime, FrameReport& report) {
    for (const Doc::Clip* clip : state.emitters) {
        const auto* emitter = std::get_if<Doc::EmitterCmd>(&clip->command);
        if (emitter == nullptr) continue;
        const int elapsed = std::max(0, state.frame - clip->start);
        int live = 0;
        for (const Particle& particle : runtime.particles) {
            if (particle.emitter == clip->id && particle.age >= 0) live++;
        }
        ReportEntity entity;
        entity.kind = "emitter";
        entity.name = clip->id;
        entity.values.push_back(
            Text("reach", std::to_string(RingReach(*emitter, elapsed)) + " px", clip));
        entity.values.push_back(Text(
            "ring phase", std::to_string((int)RingPhase(*emitter, state.frame)) + " deg", clip));
        entity.values.push_back(Integer("live", live, clip));
        entity.values.push_back(Text("cell", emitter->asset + " " + emitter->cell, clip));
        report.entities.push_back(std::move(entity));
    }
}

void AddCamera(const FrameState& state, FrameReport& report) {
    const CameraState& camera = state.camera;
    ReportEntity entity;
    entity.kind = "camera";
    entity.name = "camera";
    entity.values.push_back(Vector("eye", camera.eye, camera.from.eye));
    entity.values.push_back(Vector("at", camera.at, camera.from.at));
    entity.values.push_back(Vector("up", camera.up, camera.from.up));
    entity.values.push_back(Scalar("fov_y", camera.fov_y, camera.from.fov_y));
    entity.values.push_back(Scalar("near_z", camera.near_z, camera.from.near_z));
    entity.values.push_back(Scalar("far_z", camera.far_z, camera.from.far_z));
    entity.values.push_back(Scalar("aspect", camera.aspect_value, camera.from.aspect));
    report.entities.push_back(std::move(entity));
}

void AddLights(const FrameState& state, FrameReport& report) {
    for (std::size_t i = 0; i < state.lights.size(); i++) {
        const LightState& light = state.lights[i];
        ReportEntity entity;
        entity.kind = "light";
        entity.name = "light " + std::to_string(i);
        entity.values.push_back(Vector("direction", light.direction, light.from));
        entity.values.push_back(Vector("diffuse", light.diffuse, light.from));
        entity.values.push_back(Vector("specular", light.specular, light.from));
        report.entities.push_back(std::move(entity));
    }
}

void AddScene(const Doc::Document& document, const FrameState& state, const EvalState& runtime,
              FrameReport& report) {
    ReportEntity entity;
    entity.kind = "scene";
    entity.name = "scene";
    entity.values.push_back(
        Text("shading", NameOf(Doc::kShadingNames, (int)state.shading), state.shading_from));
    entity.values.push_back(
        Integer("sprite_split_priority", state.sprite_split_priority, state.split_from));
    if (state.beat.rate > 0) {
        entity.values.push_back(Integer("beat a", runtime.beat[0], state.beat.from));
        entity.values.push_back(Integer("beat b", runtime.beat[1], state.beat.from));
    } else {
        entity.values.push_back(Text("beat", "no beat grid", state.beat.from));
    }
    entity.values.push_back(Scalar("pulse", runtime.pulse, nullptr));
    entity.values.push_back(Scalar("jitter", runtime.jitter, state.jitter.from));
    entity.values.push_back(Integer("particle pool", (int)runtime.particles.size(), nullptr));
    entity.values.push_back(Text("rng",
                                 "seed " + std::to_string(document.rng_seed) + ", " +
                                     std::to_string(runtime.rng.Draws()) + " draw(s)",
                                 nullptr));
    report.entities.push_back(std::move(entity));
}

void AddOptions(const Doc::Document& document, const EvalState& runtime, FrameReport& report) {
    if (document.options.empty()) return;
    ReportEntity entity;
    entity.kind = "options";
    entity.name = "options";
    for (std::size_t i = 0; i < document.options.size(); i++) {
        const Doc::OptionSpec& option = document.options[i];
        const int index = SelectedChoice(option, runtime.choices, i);
        const std::string label =
            index < 0 ? std::string("(none)") : option.choices[(std::size_t)index].label;
        entity.values.push_back(Text(option.id, label, nullptr));
    }
    if (runtime.transition > 0 && runtime.transition_option >= 0) {
        entity.values.push_back(
            Text("transition", std::to_string(runtime.transition) + " frame(s) left", nullptr));
    }
    report.entities.push_back(std::move(entity));
}

void AddMarkers(const Doc::Document& document, int frame, FrameReport& report) {
    if (document.markers.empty()) return;
    ReportEntity entity;
    entity.kind = "markers";
    entity.name = "markers";
    const Doc::Marker* active = nullptr;
    const Doc::Marker* next = nullptr;
    for (const Doc::Marker& marker : document.markers) {
        if (marker.frame <= frame) active = &marker;
        if (marker.frame > frame && next == nullptr) next = &marker;
    }
    if (active != nullptr) {
        entity.values.push_back(
            Text("at " + std::to_string(active->frame), active->label, nullptr));
    }
    if (next != nullptr) {
        entity.values.push_back(
            Text("next at " + std::to_string(next->frame), next->label, nullptr));
    }
    if (!entity.values.empty()) report.entities.push_back(std::move(entity));
}

}

FrameReport BuildFrameReport(const Doc::Document& document, const FrameState& state,
                             const EvalState& runtime) {
    FrameReport report;
    report.frame = state.frame;
    report.length = document.length.value_or(0);
    report.fps = document.fps;
    AddModels(state, runtime, report);
    AddSprites(state, runtime, report);
    AddEmitters(state, runtime, report);
    AddCamera(state, report);
    AddLights(state, report);
    AddScene(document, state, runtime, report);
    AddOptions(document, runtime, report);
    AddMarkers(document, state.frame, report);
    return report;
}

}
