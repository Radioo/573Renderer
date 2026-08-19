#pragma once

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_fields.h"

#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace Preset::Doc {

inline ParamValue ToParam(bool value) {
    return value;
}
inline ParamValue ToParam(int value) {
    return value;
}
inline ParamValue ToParam(double value) {
    return value;
}
inline ParamValue ToParam(const std::string& value) {
    return value;
}
inline ParamValue ToParam(const Vec2& value) {
    return value;
}
inline ParamValue ToParam(const Vec3& value) {
    return value;
}
inline ParamValue ToParam(const std::vector<std::string>& value) {
    return value;
}
inline ParamValue ToParam(const AspectSpec& value) {
    return value;
}
inline ParamValue ToParam(const ClipTime& value) {
    return value;
}
inline ParamValue ToParam(const std::optional<ClipClock>& value) {
    return value;
}
inline ParamValue ToParam(const std::optional<Orbit>& value) {
    return value;
}
inline ParamValue ToParam(const std::optional<PulseSpec>& value) {
    return value;
}
inline ParamValue ToParam(const std::optional<Scatter>& value) {
    return value;
}
inline ParamValue ToParam(const std::optional<Burst>& value) {
    return value;
}
inline ParamValue ToParam(const std::optional<MovieTexture>& value) {
    return value;
}
inline ParamValue ToParam(const OverrideValue& value) {
    return std::visit([](const auto& held) { return ToParam(held); }, value);
}

template <class E>
    requires std::is_enum_v<E>
ParamValue ToParam(E value) {
    return (int)value;
}

template <class T> ParamValue ToParam(const std::optional<T>& value) {
    if (!value.has_value()) return {};
    return ToParam(*value);
}

inline void FromParam(const ParamValue& value, bool& out) {
    if (const auto* held = std::get_if<bool>(&value)) out = *held;
}
inline void FromParam(const ParamValue& value, int& out) {
    if (const auto* held = std::get_if<int>(&value)) out = *held;
}
inline void FromParam(const ParamValue& value, double& out) {
    if (const auto* held = std::get_if<double>(&value)) out = *held;
}
inline void FromParam(const ParamValue& value, std::string& out) {
    if (const auto* held = std::get_if<std::string>(&value)) out = *held;
}
inline void FromParam(const ParamValue& value, Vec2& out) {
    if (const auto* held = std::get_if<Vec2>(&value)) out = *held;
}
inline void FromParam(const ParamValue& value, Vec3& out) {
    if (const auto* held = std::get_if<Vec3>(&value)) out = *held;
}
inline void FromParam(const ParamValue& value, std::vector<std::string>& out) {
    if (const auto* held = std::get_if<std::vector<std::string>>(&value)) out = *held;
}
inline void FromParam(const ParamValue& value, AspectSpec& out) {
    if (const auto* held = std::get_if<AspectSpec>(&value)) out = *held;
}
inline void FromParam(const ParamValue& value, ClipTime& out) {
    if (const auto* held = std::get_if<ClipTime>(&value)) out = *held;
}
inline void FromParam(const ParamValue& value, std::optional<ClipClock>& out) {
    if (const auto* held = std::get_if<std::optional<ClipClock>>(&value)) out = *held;
}
inline void FromParam(const ParamValue& value, std::optional<Orbit>& out) {
    if (const auto* held = std::get_if<std::optional<Orbit>>(&value)) out = *held;
}
inline void FromParam(const ParamValue& value, std::optional<PulseSpec>& out) {
    if (const auto* held = std::get_if<std::optional<PulseSpec>>(&value)) out = *held;
}
inline void FromParam(const ParamValue& value, std::optional<Scatter>& out) {
    if (const auto* held = std::get_if<std::optional<Scatter>>(&value)) out = *held;
}
inline void FromParam(const ParamValue& value, std::optional<Burst>& out) {
    if (const auto* held = std::get_if<std::optional<Burst>>(&value)) out = *held;
}
inline void FromParam(const ParamValue& value, std::optional<MovieTexture>& out) {
    if (const auto* held = std::get_if<std::optional<MovieTexture>>(&value)) out = *held;
}
inline void FromParam(const ParamValue& value, OverrideValue& out) {
    if (const auto* as_bool = std::get_if<bool>(&value)) out = *as_bool;
    if (const auto* as_double = std::get_if<double>(&value)) out = *as_double;
    if (const auto* as_string = std::get_if<std::string>(&value)) out = *as_string;
    if (const auto* as_vec = std::get_if<Vec3>(&value)) out = *as_vec;
}

template <class E>
    requires std::is_enum_v<E>
void FromParam(const ParamValue& value, E& out) {
    if (const auto* held = std::get_if<int>(&value)) out = (E)*held;
}

template <class T> void FromParam(const ParamValue& value, std::optional<T>& out) {
    if (std::holds_alternative<std::monostate>(value)) {
        out.reset();
        return;
    }
    T held = {};
    FromParam(value, held);
    out = held;
}

template <class C, auto Member> constexpr FieldDesc Field(FieldDesc desc) {
    desc.get = [](const Command& command) -> ParamValue {
        return ToParam(std::get<C>(command).*Member);
    };
    desc.set = [](Command& command, const ParamValue& value) {
        FromParam(value, std::get<C>(command).*Member);
    };
    return desc;
}

inline constexpr Range kUnitInterval = {.min = 0.0F, .max = 1.0F, .step = 0.005F};
inline constexpr Range kPriority = {.min = 0.0F, .max = 64.0F, .step = 1.0F};
inline constexpr Range kFrameCount = {.min = 0.0F, .max = 36000.0F, .step = 1.0F, .soft = true};
inline constexpr Range kSpeed = {.min = 0.0F, .max = 8.0F, .step = 0.005F, .soft = true};
inline constexpr Range kDegrees = {.min = -360.0F, .max = 360.0F, .soft = true};

std::span<const FieldDesc> SpriteDrawFields();
std::span<const FieldDesc> SpriteAnimateFields();
std::span<const FieldDesc> SpriteScrollFields();
std::span<const FieldDesc> EmitterFields();

std::span<const FieldDesc> ModelDrawFields();
std::span<const FieldDesc> ModelMotionFields();
std::span<const FieldDesc> CameraSetFields();
std::span<const FieldDesc> LightSetFields();
std::span<const FieldDesc> CameraEaseFields();
std::span<const FieldDesc> ModelEaseFields();
std::span<const FieldDesc> CameraMotionFields();
std::span<const FieldDesc> PolyTileGridFields();

}
