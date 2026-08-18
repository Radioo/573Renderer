#include "preset/eval/eval_models.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/eval_tween.h"
#include "preset/eval/frame_state.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace Preset::Eval {

namespace {

Vec3f ToVec3f(const Doc::Vec3& value) {
    return {(float)value[0], (float)value[1], (float)value[2]};
}

TweenValue ScalarOf(float value) {
    TweenValue out;
    out.kind = TweenValue::Kind::Scalar;
    out.scalar = value;
    return out;
}

TweenValue VectorOf(const Vec3f& value) {
    TweenValue out;
    out.kind = TweenValue::Kind::Vector;
    out.vector = value;
    return out;
}

TweenValue IntegerOf(int value) {
    TweenValue out;
    out.kind = TweenValue::Kind::Integer;
    out.integer = value;
    return out;
}

void RecordScalar(std::vector<MaterialWrite>& writes, WriteKind kind, int model, bool legacy,
                  float value) {
    if (model < 0) return;
    writes.push_back(
        MaterialWrite{.kind = kind, .model = model, .legacy = legacy, .scalar = value});
}

void RecordInteger(std::vector<MaterialWrite>& writes, WriteKind kind, int model, bool legacy,
                   int value) {
    if (model < 0) return;
    writes.push_back(
        MaterialWrite{.kind = kind, .model = model, .legacy = legacy, .integer = value});
}

void RecordVector(std::vector<MaterialWrite>& writes, WriteKind kind, int model, bool legacy,
                  const Vec3f& value) {
    if (model < 0) return;
    writes.push_back(
        MaterialWrite{.kind = kind, .model = model, .legacy = legacy, .vector = value});
}

void SampleModelKeys(const Doc::Clip& clip, int frame, ModelSlot& slot, int model_index,
                     std::vector<MaterialWrite>& writes) {
    if (clip.keys.empty()) return;
    const int clip_frame = frame - clip.start;
    TweenValue sampled;
    if (SampleKeys(clip.keys, "anim_speed", clip_frame, ScalarOf(slot.anim_speed), sampled)) {
        slot.anim_speed = sampled.scalar;
        slot.from.anim_speed = &clip;
        RecordScalar(writes, WriteKind::AnimSpeed, model_index, true, slot.anim_speed);
    }
    if (SampleKeys(clip.keys, "blend_mode", clip_frame, IntegerOf(slot.blend_mode), sampled)) {
        slot.blend_mode = sampled.integer;
        slot.from.blend_mode = &clip;
        RecordInteger(writes, WriteKind::BlendMode, model_index, true, slot.blend_mode);
    }
    if (SampleKeys(clip.keys, "alpha", clip_frame, ScalarOf(slot.alpha), sampled)) {
        slot.alpha = sampled.scalar;
        slot.from.alpha = &clip;
        RecordScalar(writes, WriteKind::Alpha, model_index, false, slot.alpha);
    }
    if (SampleKeys(clip.keys, "scale", clip_frame, VectorOf(slot.scale), sampled)) {
        slot.scale = sampled.vector;
        slot.from.scale = &clip;
        RecordVector(writes, WriteKind::ModelScale, model_index, false, slot.scale);
    }
    if (SampleKeys(clip.keys, "position", clip_frame, VectorOf(slot.position), sampled)) {
        slot.position = sampled.vector;
        slot.from.position = &clip;
    }
    if (SampleKeys(clip.keys, "rotation", clip_frame, VectorOf(slot.rotation), sampled)) {
        slot.rotation = sampled.vector;
        slot.from.rotation = &clip;
    }
    if (SampleKeys(clip.keys, "spin_per_frame", clip_frame, VectorOf(slot.spin_per_frame),
                   sampled)) {
        slot.spin_per_frame = sampled.vector;
        slot.from.spin_per_frame = &clip;
    }
}

}

void ApplyModelDraw(const Doc::Clip& clip, const Doc::ModelDraw& command, int frame,
                    ModelSlot& slot, int model_index, std::vector<MaterialWrite>& writes,
                    bool with_keys) {
    slot.visible = true;
    slot.asset = command.asset;
    if (!command.model.empty()) slot.name = command.model;
    slot.blend_mode = (int)command.blend_mode;
    slot.alpha = (float)command.alpha;
    slot.anim_speed = (float)command.anim_speed;
    slot.position = ToVec3f(command.position);
    slot.rotation = ToVec3f(command.rotation);
    slot.scale = ToVec3f(command.scale);
    slot.spin_per_frame = ToVec3f(command.spin_per_frame);
    slot.draw_start = clip.start;
    slot.from = ModelOrigin{.visible = &clip,
                            .position = &clip,
                            .rotation = &clip,
                            .scale = &clip,
                            .alpha = &clip,
                            .anim_speed = &clip,
                            .blend_mode = &clip,
                            .spin_per_frame = &clip,
                            .motion = slot.from.motion};
    if (with_keys) SampleModelKeys(clip, frame, slot, model_index, writes);
}

void ApplyModelTween(const Doc::Clip& clip, int frame, ModelSlot& slot, int model_index,
                     std::vector<MaterialWrite>& writes) {
    SampleModelKeys(clip, frame, slot, model_index, writes);
}

void ApplyModelMotion(const Doc::Clip& clip, const Doc::ModelMotionCmd& command, ModelSlot& slot) {
    if (command.orbit.has_value()) {
        slot.has_orbit = true;
        slot.orbit = OrbitState{.radius = (float)command.orbit->radius,
                                .rate = (float)command.orbit->rate_rad_per_frame,
                                .center_x = (float)command.orbit->center[0],
                                .center_y = (float)command.orbit->center[1],
                                .z_start = (float)command.orbit->z_start,
                                .z_per_frame = (float)command.orbit->z_per_frame,
                                .z_min = (float)command.orbit->z_min};
    }
    if (command.spin_kick != 0.0) slot.spin_kick = (float)command.spin_kick;
    if (command.spin_kick_decay != 0.0) slot.spin_kick_decay = (float)command.spin_kick_decay;
    if (command.pulse.has_value()) {
        slot.has_pulse = true;
        slot.pulse = PulseState{.grid = command.pulse->grid,
                                .scale_odd = (float)command.pulse->scale_odd,
                                .scale_even = (float)command.pulse->scale_even,
                                .frames = command.pulse->frames};
    }
    slot.motion_start = clip.start;
    slot.from.motion = &clip;
}

Vec3f OrbitPosition(const OrbitState& orbit, int frame) {
    const auto t = (float)frame;
    const float angle = t * orbit.rate;
    return {orbit.center_x + (std::cos(angle) * orbit.radius),
            orbit.center_y + (std::sin(angle) * orbit.radius),
            std::max(orbit.z_min, orbit.z_start - (t * orbit.z_per_frame))};
}

bool ShakenBy(const JitterState& jitter, const std::string& model) {
    if (!jitter.active) return false;
    return jitter.models.empty() || std::ranges::find(jitter.models, model) != jitter.models.end();
}

Vec3f PlacedPosition(const ModelSlot& slot, const JitterState& jitter, float shake, int frame) {
    const bool shaken = ShakenBy(jitter, slot.name);
    if (shaken && jitter.mode == Doc::JitterMode::Set) return {shake, shake, 0.0F};
    if (shaken) return {slot.position[0] + shake, slot.position[1] + shake, slot.position[2]};
    if (slot.has_orbit && slot.orbit.radius > 0.0F) return OrbitPosition(slot.orbit, frame);
    return slot.position;
}

bool ModelMoves(const ModelSlot& slot, bool jitter_active, bool choice_driven) {
    if (jitter_active || choice_driven) return true;
    if (slot.has_orbit && slot.orbit.radius > 0.0F) return true;
    if (slot.spin_kick > 0.0F) return true;
    return slot.spin_per_frame != Vec3f{0.0F, 0.0F, 0.0F};
}

}
