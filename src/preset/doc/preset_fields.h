#pragma once

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/preset_params.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace Preset::Doc {

enum class FieldKind : uint8_t {
    Bool,
    Int,
    Float,
    Enum,
    String,
    Vec2,
    Vec3,
    StringList,
    Aspect,
    Clock,
    ClipTimeField,
    OrbitField,
    PulseField,
    ScatterField,
    OverrideField,
};

struct FieldDesc {
    std::string_view id;
    FieldKind kind = FieldKind::Float;
    Preset::Range range = {};
    std::string_view unit = {};
    std::string_view help = {};
    bool tweenable = false;
    bool required = false;
    std::span<const std::string_view> enum_names = {};
    ParamValue (*get)(const Command&) = nullptr;
    void (*set)(Command&, const ParamValue&) = nullptr;
};

std::span<const FieldDesc> FieldsFor(CommandType type);

std::span<const FieldDesc> KeyFieldsFor(CommandType type);

const FieldDesc* FindField(std::span<const FieldDesc> fields, std::string_view id);

const Command& DefaultCommand(CommandType type);

}
