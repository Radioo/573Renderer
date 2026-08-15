#include "preset/preset_host_params.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/eval_scene.h"
#include "preset/eval/eval_tween.h"
#include "preset/eval/frame_state.h"
#include "preset/preset_host.h"
#include "preset/preset_params.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <utility>
#include <string>
#include <string_view>
#include <vector>

namespace PresetHost {

namespace {

using Preset::Eval::FrameState;
using Preset::Eval::TweenValue;

struct FieldRow {
    std::string_view field;
    std::string_view label;
    Preset::ValueKind kind;
    Preset::Range range;
    std::string_view unit;
    std::span<const std::string_view> enums;
};

constexpr Preset::Range kUnit = {.min = 0.0F, .max = 1.0F, .step = 0.01F, .soft = false};
constexpr Preset::Range kFree = {.min = -1000.0F, .max = 1000.0F, .step = 0.01F, .soft = true};
constexpr Preset::Range kSpeed = {.min = 0.0F, .max = 8.0F, .step = 0.01F, .soft = true};
constexpr Preset::Range kPixels = {.min = -4096.0F, .max = 4096.0F, .step = 1.0F, .soft = true};
constexpr Preset::Range kPriority = {.min = 0.0F, .max = 255.0F, .step = 1.0F, .soft = false};
constexpr Preset::Range kAngle = {
    .min = -12.566371F, .max = 12.566371F, .step = 0.001F, .soft = true};
constexpr Preset::Range kDepth = {.min = 0.001F, .max = 4096.0F, .step = 0.1F, .soft = true};

const std::array<FieldRow, 7> kModelFields = {
    FieldRow{.field = "blend_mode",
             .label = "Blend mode",
             .kind = Preset::ValueKind::Enum,
             .range = {},
             .unit = {},
             .enums = Preset::Doc::kModelBlendNames},
    FieldRow{.field = "alpha",
             .label = "Alpha",
             .kind = Preset::ValueKind::Float,
             .range = kUnit,
             .unit = {},
             .enums = {}},
    FieldRow{.field = "anim_speed",
             .label = "Animation speed",
             .kind = Preset::ValueKind::Float,
             .range = kSpeed,
             .unit = "ticks per frame",
             .enums = {}},
    FieldRow{.field = "position",
             .label = "Position",
             .kind = Preset::ValueKind::Vec3,
             .range = kFree,
             .unit = "units",
             .enums = {}},
    FieldRow{.field = "rotation",
             .label = "Rotation",
             .kind = Preset::ValueKind::Vec3,
             .range = kAngle,
             .unit = "radians",
             .enums = {}},
    FieldRow{.field = "scale",
             .label = "Scale",
             .kind = Preset::ValueKind::Vec3,
             .range = kSpeed,
             .unit = {},
             .enums = {}},
    FieldRow{.field = "spin_per_frame",
             .label = "Spin per frame",
             .kind = Preset::ValueKind::Vec3,
             .range = kAngle,
             .unit = "radians per frame",
             .enums = {}},
};

const std::array<FieldRow, 6> kSpriteFields = {
    FieldRow{.field = "x",
             .label = "X",
             .kind = Preset::ValueKind::Float,
             .range = kPixels,
             .unit = "px",
             .enums = {}},
    FieldRow{.field = "y",
             .label = "Y",
             .kind = Preset::ValueKind::Float,
             .range = kPixels,
             .unit = "px",
             .enums = {}},
    FieldRow{.field = "alpha",
             .label = "Alpha",
             .kind = Preset::ValueKind::Float,
             .range = kUnit,
             .unit = {},
             .enums = {}},
    FieldRow{.field = "scale",
             .label = "Scale",
             .kind = Preset::ValueKind::Float,
             .range = kSpeed,
             .unit = {},
             .enums = {}},
    FieldRow{.field = "blend",
             .label = "Blend",
             .kind = Preset::ValueKind::Enum,
             .range = {},
             .unit = {},
             .enums = Preset::Doc::kSpriteBlendNames},
    FieldRow{.field = "priority",
             .label = "Priority",
             .kind = Preset::ValueKind::Int,
             .range = kPriority,
             .unit = {},
             .enums = {}},
};

const std::array<FieldRow, 6> kCameraFields = {
    FieldRow{.field = "eye",
             .label = "Eye",
             .kind = Preset::ValueKind::Vec3,
             .range = kFree,
             .unit = "units",
             .enums = {}},
    FieldRow{.field = "at",
             .label = "Look at",
             .kind = Preset::ValueKind::Vec3,
             .range = kFree,
             .unit = "units",
             .enums = {}},
    FieldRow{.field = "up",
             .label = "Up",
             .kind = Preset::ValueKind::Vec3,
             .range = kFree,
             .unit = {},
             .enums = {}},
    FieldRow{.field = "fov_y",
             .label = "Vertical FOV",
             .kind = Preset::ValueKind::Float,
             .range = kAngle,
             .unit = "radians",
             .enums = {}},
    FieldRow{.field = "near_z",
             .label = "Near plane",
             .kind = Preset::ValueKind::Float,
             .range = kDepth,
             .unit = "units",
             .enums = {}},
    FieldRow{.field = "far_z",
             .label = "Far plane",
             .kind = Preset::ValueKind::Float,
             .range = kDepth,
             .unit = "units",
             .enums = {}},
};

const std::array<FieldRow, 3> kLightFields = {
    FieldRow{.field = "direction",
             .label = "Direction",
             .kind = Preset::ValueKind::Vec3,
             .range = kFree,
             .unit = {},
             .enums = {}},
    FieldRow{.field = "diffuse",
             .label = "Diffuse",
             .kind = Preset::ValueKind::Color,
             .range = kUnit,
             .unit = {},
             .enums = {}},
    FieldRow{.field = "specular",
             .label = "Specular",
             .kind = Preset::ValueKind::Color,
             .range = kUnit,
             .unit = {},
             .enums = {}},
};

Preset::Value ToValue(const TweenValue& value) {
    Preset::Value out;
    switch (value.kind) {
    case TweenValue::Kind::Scalar:
        out.f = {value.scalar, 0.0F, 0.0F};
        out.i = value.integer;
        break;
    case TweenValue::Kind::Vector:
        out.f = value.vector;
        break;
    case TweenValue::Kind::Integer:
        out.f = {(float)value.integer, 0.0F, 0.0F};
        out.i = value.integer;
        break;
    }
    return out;
}

bool SameRow(const Preset::Value& a, const Preset::Value& b, Preset::ValueKind kind) {
    if (kind == Preset::ValueKind::Enum || kind == Preset::ValueKind::Int) return a.i == b.i;
    return a.f == b.f;
}

void AddRow(const std::string& id, const std::string& group, const FieldRow& field,
            const FrameState& effective, const FrameState& base,
            const std::vector<Override>& overrides, std::vector<ParamView>& out) {
    TweenValue current;
    if (!Preset::Eval::ReadTarget(id, effective, current)) return;
    TweenValue fallback = current;
    Preset::Eval::ReadTarget(id, base, fallback);

    const Preset::Value value = ToValue(current);
    const Preset::Value original = ToValue(fallback);
    ParamView view;
    view.id = id;
    view.label = std::string(field.label);
    view.group = group;
    view.unit = std::string(field.unit);
    view.kind = (int)field.kind;
    view.min = field.range.min;
    view.max = field.range.max;
    view.step = field.range.step;
    view.soft = field.range.soft;
    view.value = value.f;
    view.ivalue = value.i;
    view.fallback = original.f;
    view.ifallback = original.i;
    view.overridden =
        std::ranges::any_of(overrides, [&id](const Override& held) { return held.id == id; }) ||
        !SameRow(value, original, field.kind);
    for (const std::string_view label : field.enums)
        view.enum_labels.emplace_back(label);
    out.push_back(std::move(view));
}

std::string ModelGroup(const std::string& name) {
    return "Model " + name;
}

std::string SpriteGroup(const std::string& name) {
    return "2D layer " + name;
}

std::string LightGroup(std::size_t index) {
    return "Light " + std::to_string(index);
}

TweenValue OverrideFor(const std::string& id, const std::array<float, 3>& value, int ivalue,
                       const FrameState& effective) {
    TweenValue current;
    if (!Preset::Eval::ReadTarget(id, effective, current)) return current;
    TweenValue next = current;
    switch (current.kind) {
    case TweenValue::Kind::Scalar:
        next.scalar = value[0];
        next.integer = ivalue;
        break;
    case TweenValue::Kind::Vector:
        next.vector = value;
        break;
    case TweenValue::Kind::Integer:
        next.integer = ivalue;
        break;
    }
    return next;
}

}

std::vector<ParamView> ListParamViews(const FrameState& effective, const FrameState& base,
                                      const std::vector<Override>& overrides) {
    std::vector<ParamView> out;
    for (const Preset::Eval::ModelSlot& slot : effective.models) {
        for (const FieldRow& field : kModelFields) {
            AddRow("model[" + slot.name + "]." + std::string(field.field), ModelGroup(slot.name),
                   field, effective, base, overrides, out);
        }
    }
    for (const Preset::Eval::SpriteSlot& slot : effective.sprites) {
        for (const FieldRow& field : kSpriteFields) {
            AddRow("sprite[" + slot.name + "]." + std::string(field.field), SpriteGroup(slot.name),
                   field, effective, base, overrides, out);
        }
    }
    for (const FieldRow& field : kCameraFields) {
        AddRow("camera." + std::string(field.field), "Camera", field, effective, base, overrides,
               out);
    }
    for (std::size_t i = 0; i < effective.lights.size(); i++) {
        for (const FieldRow& field : kLightFields) {
            AddRow("light[" + std::to_string(i) + "]." + std::string(field.field), LightGroup(i),
                   field, effective, base, overrides, out);
        }
    }
    AddRow("shading", "Scene",
           FieldRow{.field = "shading",
                    .label = "Shading",
                    .kind = Preset::ValueKind::Enum,
                    .range = {},
                    .unit = {},
                    .enums = Preset::Doc::kShadingNames},
           effective, base, overrides, out);
    AddRow("sprite_split_priority", "Scene",
           FieldRow{.field = "sprite_split_priority",
                    .label = "2D split priority",
                    .kind = Preset::ValueKind::Int,
                    .range = kPriority,
                    .unit = {},
                    .enums = {}},
           effective, base, overrides, out);
    return out;
}

Preset::Doc::Document WithOverrides(const Preset::Doc::Document& base,
                                    const std::vector<Override>& overrides) {
    Preset::Doc::Document out = base;
    if (overrides.empty()) return out;
    Preset::Doc::Track track;
    track.id = "host_overrides";
    track.name = "Overrides";
    track.kind = Preset::Doc::TrackKind::Scene;
    for (std::size_t i = 0; i < overrides.size(); i++) {
        Preset::Doc::Clip clip;
        clip.id = "host_override_" + std::to_string(i);
        clip.start = 0;
        clip.command =
            Preset::Doc::ParamOverrideCmd{.id = overrides[i].id, .value = overrides[i].value};
        track.clips.push_back(std::move(clip));
    }
    out.tracks.push_back(std::move(track));
    return out;
}

bool WriteOverride(std::vector<Override>& overrides, const std::string& id,
                   const std::array<float, 3>& value, int ivalue, const FrameState& effective) {
    TweenValue current;
    if (!Preset::Eval::ReadTarget(id, effective, current)) return false;
    const TweenValue next = OverrideFor(id, value, ivalue, effective);
    Preset::Doc::OverrideValue stored;
    if (next.kind == TweenValue::Kind::Vector) {
        stored = Preset::Doc::Vec3{next.vector[0], next.vector[1], next.vector[2]};
    } else if (next.kind == TweenValue::Kind::Integer) {
        stored = (double)next.integer;
    } else {
        stored = (double)next.scalar;
    }
    for (Override& held : overrides) {
        if (held.id != id) continue;
        held.value = stored;
        return true;
    }
    overrides.push_back(Override{.id = id, .value = stored});
    return true;
}

void ClearOverride(std::vector<Override>& overrides, const std::string& id) {
    std::erase_if(overrides, [&id](const Override& held) { return held.id == id; });
}

void ClearOverrideGroup(std::vector<Override>& overrides, const std::string& group,
                        const FrameState& effective) {
    std::vector<std::string> ids;
    for (const ParamView& view : ListParamViews(effective, effective, overrides)) {
        if (view.group == group) ids.push_back(view.id);
    }
    for (const std::string& id : ids)
        ClearOverride(overrides, id);
}

}
