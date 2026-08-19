#include "preset/eval/eval_scene.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/eval_tween.h"
#include "preset/eval/frame_state.h"

#include <algorithm>
#include <cmath>
#include <array>
#include <cstddef>
#include <string_view>
#include <variant>
#include <vector>

namespace Preset::Eval {

namespace {

struct TargetPath {
    std::string_view scope;
    std::string_view name;
    std::string_view field;
    int index = -1;
};

bool SplitTarget(std::string_view id, TargetPath& out) {
    const std::size_t bracket = id.find('[');
    if (bracket == std::string_view::npos) {
        const std::size_t dot = id.find('.');
        if (dot == std::string_view::npos) {
            out.scope = id;
            return true;
        }
        out.scope = id.substr(0, dot);
        out.field = id.substr(dot + 1);
        return true;
    }
    const std::size_t close = id.find(']', bracket);
    if (close == std::string_view::npos) return false;
    out.scope = id.substr(0, bracket);
    out.name = id.substr(bracket + 1, close - bracket - 1);
    if (close + 1 < id.size() && id[close + 1] == '.') out.field = id.substr(close + 2);
    if (!out.name.empty() && (out.name[0] >= '0' && out.name[0] <= '9')) {
        out.index = 0;
        for (const char digit : out.name)
            out.index = (out.index * 10) + (digit - '0');
    }
    return true;
}

ModelSlot* FindModel(FrameState& state, std::string_view name) {
    for (ModelSlot& slot : state.models) {
        if (slot.name == name) return &slot;
    }
    return nullptr;
}

const ModelSlot* FindModel(const FrameState& state, std::string_view name) {
    for (const ModelSlot& slot : state.models) {
        if (slot.name == name) return &slot;
    }
    return nullptr;
}

SpriteSlot* FindSprite(FrameState& state, std::string_view name) {
    for (SpriteSlot& slot : state.sprites) {
        if (slot.name == name) return &slot;
    }
    return nullptr;
}

const SpriteSlot* FindSprite(const FrameState& state, std::string_view name) {
    for (const SpriteSlot& slot : state.sprites) {
        if (slot.name == name) return &slot;
    }
    return nullptr;
}

TweenValue Scalar(float value) {
    return TweenValue{.kind = TweenValue::Kind::Scalar, .scalar = value};
}

TweenValue Vector(const Vec3f& value) {
    return TweenValue{.kind = TweenValue::Kind::Vector, .vector = value};
}

TweenValue Integer(int value) {
    return TweenValue{.kind = TweenValue::Kind::Integer, .integer = value};
}

bool ReadModel(const ModelSlot& slot, std::string_view field, TweenValue& out) {
    if (field == "alpha") {
        out = Scalar(slot.alpha);
    } else if (field == "anim_speed") {
        out = Scalar(slot.anim_speed);
    } else if (field == "blend_mode") {
        out = Integer(slot.blend_mode);
    } else if (field == "position") {
        out = Vector(slot.position);
    } else if (field == "rotation") {
        out = Vector(slot.rotation);
    } else if (field == "scale") {
        out = Vector(slot.scale);
    } else if (field == "spin_per_frame") {
        out = Vector(slot.spin_per_frame);
    } else {
        return false;
    }
    return true;
}

bool WriteModel(ModelSlot& slot, std::string_view field, const TweenValue& value,
                const Doc::Clip* origin) {
    if (field == "alpha") {
        slot.alpha = value.scalar;
        slot.from.alpha = origin;
    } else if (field == "anim_speed") {
        slot.anim_speed = value.scalar;
        slot.from.anim_speed = origin;
    } else if (field == "blend_mode") {
        slot.blend_mode = value.integer;
        slot.from.blend_mode = origin;
    } else if (field == "position") {
        slot.position = value.vector;
        slot.from.position = origin;
    } else if (field == "rotation") {
        slot.rotation = value.vector;
        slot.from.rotation = origin;
    } else if (field == "scale") {
        slot.scale = value.vector;
        slot.from.scale = origin;
    } else if (field == "spin_per_frame") {
        slot.spin_per_frame = value.vector;
        slot.from.spin_per_frame = origin;
    } else {
        return false;
    }
    return true;
}

bool ReadSprite(const SpriteSlot& slot, std::string_view field, TweenValue& out) {
    if (field == "x") {
        out = Scalar(slot.x);
    } else if (field == "y") {
        out = Scalar(slot.y);
    } else if (field == "alpha") {
        out = Scalar(slot.alpha);
    } else if (field == "scale") {
        out = Scalar(slot.scale);
    } else if (field == "blend") {
        out = Integer(slot.blend);
    } else if (field == "priority") {
        out = Integer(slot.priority);
    } else {
        return false;
    }
    return true;
}

bool WriteSprite(SpriteSlot& slot, std::string_view field, const TweenValue& value,
                 const Doc::Clip* origin) {
    if (field == "x") {
        slot.x = value.scalar;
        slot.from.x = origin;
    } else if (field == "y") {
        slot.y = value.scalar;
        slot.from.y = origin;
    } else if (field == "alpha") {
        slot.alpha = value.scalar;
        slot.from.alpha = origin;
    } else if (field == "scale") {
        slot.scale = value.scalar;
        slot.from.scale = origin;
    } else if (field == "blend") {
        slot.blend = value.integer;
        slot.from.blend = origin;
    } else if (field == "priority") {
        slot.priority = value.integer;
        slot.from.priority = origin;
    } else {
        return false;
    }
    return true;
}

bool ReadCamera(const CameraState& camera, std::string_view field, TweenValue& out) {
    if (field == "eye") {
        out = Vector(camera.eye);
    } else if (field == "at") {
        out = Vector(camera.at);
    } else if (field == "up") {
        out = Vector(camera.up);
    } else if (field == "fov_y") {
        out = Scalar(camera.fov_y);
    } else if (field == "near_z") {
        out = Scalar(camera.near_z);
    } else if (field == "far_z") {
        out = Scalar(camera.far_z);
    } else {
        return false;
    }
    return true;
}

bool WriteCamera(CameraState& camera, std::string_view field, const TweenValue& value,
                 const Doc::Clip* origin) {
    if (field == "eye") {
        camera.eye = value.vector;
        camera.from.eye = origin;
    } else if (field == "at") {
        camera.at = value.vector;
        camera.from.at = origin;
    } else if (field == "up") {
        camera.up = value.vector;
        camera.from.up = origin;
    } else if (field == "fov_y") {
        camera.fov_y = value.scalar;
        camera.from.fov_y = origin;
    } else if (field == "near_z") {
        camera.near_z = value.scalar;
        camera.from.near_z = origin;
    } else if (field == "far_z") {
        camera.far_z = value.scalar;
        camera.from.far_z = origin;
    } else {
        return false;
    }
    return true;
}

bool ReadFog(const FogState& fog, std::string_view field, TweenValue& out) {
    if (field == "enabled") {
        out = Integer(fog.enabled ? 1 : 0);
    } else if (field == "color") {
        out = Vector(fog.color);
    } else if (field == "start") {
        out = Scalar(fog.start);
    } else if (field == "end") {
        out = Scalar(fog.end);
    } else if (field == "density") {
        out = Scalar(fog.density);
    } else {
        return false;
    }
    return true;
}

bool WriteFog(FogState& fog, std::string_view field, const TweenValue& value,
              const Doc::Clip* origin) {
    if (field == "enabled") {
        fog.enabled = value.integer != 0;
    } else if (field == "color") {
        fog.color = value.vector;
    } else if (field == "start") {
        fog.start = value.scalar;
    } else if (field == "end") {
        fog.end = value.scalar;
    } else if (field == "density") {
        fog.density = value.scalar;
    } else {
        return false;
    }
    fog.from = origin;
    return true;
}

bool ReadLight(const LightState& light, std::string_view field, TweenValue& out) {
    if (field == "direction") {
        out = Vector(light.direction);
    } else if (field == "diffuse") {
        out = Vector(light.diffuse);
    } else if (field == "specular") {
        out = Vector(light.specular);
    } else if (field == "ambient") {
        out = Vector(light.ambient);
    } else {
        return false;
    }
    return true;
}

bool WriteLight(LightState& light, std::string_view field, const TweenValue& value,
                const Doc::Clip* origin) {
    light.from = origin;
    if (field == "direction") {
        light.direction = value.vector;
    } else if (field == "diffuse") {
        light.diffuse = value.vector;
    } else if (field == "specular") {
        light.specular = value.vector;
    } else if (field == "ambient") {
        light.ambient = value.vector;
    } else {
        return false;
    }
    return true;
}

std::array<int, 3> ClearCycleLevels(const Doc::ClearCycleCmd& cycle, int frame) {
    std::array<int, 3> levels = {0, 0, 0};
    for (std::size_t i = 0; i < levels.size(); i++)
        levels[i] = (int)cycle.base[i];

    if (cycle.strobe_period > 0) {
        const int phase = frame % cycle.strobe_period;
        const int shifted = (frame + cycle.strobe_window_b_offset) % cycle.strobe_period;
        const bool inside = phase < cycle.strobe_window_a || shifted < cycle.strobe_window_b;
        const bool skipped = cycle.strobe_skip_every > 0 && (frame % cycle.strobe_skip_every) == 0;
        if (inside && !skipped) {
            for (std::size_t i = 0; i < levels.size(); i++)
                levels[i] = (int)cycle.strobe_color[i];
        }
    }

    if (cycle.ramp_period > 0 && cycle.ramp_length > 0) {
        const int phase = frame % cycle.ramp_period;
        if (phase < cycle.ramp_length) {
            const int grey = ((cycle.ramp_length - phase) * cycle.ramp_peak) / cycle.ramp_length;
            levels = {grey, grey, grey};
        }
    }
    return levels;
}

}

Vec3f ClearCycleColor(const Doc::ClearCycleCmd& cycle, int frame) {
    const std::array<int, 3> levels = ClearCycleLevels(cycle, frame);
    Vec3f out = {0.0F, 0.0F, 0.0F};
    for (std::size_t i = 0; i < out.size(); i++)
        out[i] = (float)levels[i] / 255.0F;
    return out;
}

int BeatIndex(const BeatState& beat, int frame, int offset) {
    return (beat.rate * (frame - offset)) / std::max(1, beat.span);
}

bool PulseActive(const BeatState& beat, const PulseState& pulse) {
    return beat.rate > 0 && (pulse.scale_odd != 1.0F || pulse.scale_even != 1.0F);
}

float PulseFactor(const BeatState& beat, const PulseState& pulse, const std::array<int, 2>& indices,
                  const std::array<int, 2>& since) {
    if (!PulseActive(beat, pulse)) return 1.0F;
    const auto grid = (std::size_t)(pulse.grid == Doc::Grid::B ? 1 : 0);
    const int frames = std::max(1, pulse.frames);
    const int elapsed = since[grid];
    if (elapsed >= frames) return 1.0F;
    const float from = ((indices[grid] & 1) != 0) ? pulse.scale_odd : pulse.scale_even;
    return from - ((from - 1.0F) * (float)elapsed / (float)frames);
}

bool ReadTarget(std::string_view id, const FrameState& state, TweenValue& out) {
    TargetPath path;
    if (!SplitTarget(id, path)) return false;
    if (path.scope == "model") {
        const ModelSlot* slot = FindModel(state, path.name);
        return slot != nullptr && ReadModel(*slot, path.field, out);
    }
    if (path.scope == "sprite") {
        const SpriteSlot* slot = FindSprite(state, path.name);
        return slot != nullptr && ReadSprite(*slot, path.field, out);
    }
    if (path.scope == "camera") return ReadCamera(state.camera, path.field, out);
    if (path.scope == "fog") return ReadFog(state.fog, path.field, out);
    if (path.scope == "light") {
        if (path.index < 0 || (std::size_t)path.index >= state.lights.size()) return false;
        return ReadLight(state.lights[(std::size_t)path.index], path.field, out);
    }
    if (path.scope == "sprite_split_priority") {
        out = Integer(state.sprite_split_priority);
        return true;
    }
    if (path.scope == "shading") {
        out = Integer((int)state.shading);
        return true;
    }
    if (path.scope == "clear_color") {
        out = Vector(state.clear_color);
        return true;
    }
    return false;
}

bool WriteTarget(std::string_view id, const TweenValue& value, FrameState& state,
                 const Doc::Clip* origin) {
    TargetPath path;
    if (!SplitTarget(id, path)) return false;
    if (path.scope == "model") {
        ModelSlot* slot = FindModel(state, path.name);
        return slot != nullptr && WriteModel(*slot, path.field, value, origin);
    }
    if (path.scope == "sprite") {
        SpriteSlot* slot = FindSprite(state, path.name);
        return slot != nullptr && WriteSprite(*slot, path.field, value, origin);
    }
    if (path.scope == "camera") return WriteCamera(state.camera, path.field, value, origin);
    if (path.scope == "fog") return WriteFog(state.fog, path.field, value, origin);
    if (path.scope == "light") {
        if (path.index < 0 || (std::size_t)path.index >= state.lights.size()) return false;
        return WriteLight(state.lights[(std::size_t)path.index], path.field, value, origin);
    }
    if (path.scope == "sprite_split_priority") {
        state.sprite_split_priority = value.integer;
        state.split_from = origin;
        return true;
    }
    if (path.scope == "shading") {
        state.shading = (Doc::Shading)value.integer;
        state.shading_from = origin;
        return true;
    }
    if (path.scope == "clear_color") {
        state.clear_color = value.vector;
        state.clear_from = origin;
        return true;
    }
    return false;
}

TweenValue OverrideToTween(const Doc::OverrideValue& value) {
    TweenValue out;
    if (const auto* held = std::get_if<double>(&value)) {
        TweenValue scalar = Scalar((float)*held);
        scalar.integer = (int)std::lround(*held);
        return scalar;
    }
    if (const auto* held = std::get_if<bool>(&value)) return Integer(*held ? 1 : 0);
    if (const auto* held = std::get_if<Doc::Vec3>(&value)) {
        Vec3f vector = {0.0F, 0.0F, 0.0F};
        for (std::size_t i = 0; i < vector.size(); i++)
            vector[i] = (float)(*held)[i];
        return Vector(vector);
    }
    return out;
}

}
