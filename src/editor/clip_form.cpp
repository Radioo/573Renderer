#include "editor/clip_form.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_fields.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <numbers>
#include <string_view>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace Editor {

namespace Doc = Preset::Doc;

namespace {

std::string Number(double value) {
    std::array<char, 48> buffer = {};
    if (value == std::floor(value) && std::abs(value) < 1.0e9) {
        (void)std::snprintf(buffer.data(), buffer.size(), "%.0f", value);
        return buffer.data();
    }
    (void)std::snprintf(buffer.data(), buffer.size(), "%.4f", value);
    return buffer.data();
}

std::string Degrees(double radians) {
    std::array<char, 32> buffer = {};
    (void)std::snprintf(buffer.data(), buffer.size(), "%.1f",
                        radians * 180.0 / std::numbers::pi_v<double>);
    return buffer.data();
}

std::string EnumText(const Doc::FieldDesc& field, int value) {
    const std::string_view name = Doc::NameForIndex(field.enum_names, value);
    if (name.empty()) return std::to_string(value);
    return std::string(name);
}

bool RadianField(const Doc::FieldDesc& field) {
    return field.unit == "rad" || field.unit == "rad/frame";
}

double LowestOf(const Doc::ParamValue& value) {
    if (const auto* number = std::get_if<double>(&value)) return *number;
    if (const auto* whole = std::get_if<int>(&value)) return (double)*whole;
    if (const auto* pair = std::get_if<Doc::Vec2>(&value)) return std::min((*pair)[0], (*pair)[1]);
    if (const auto* triple = std::get_if<Doc::Vec3>(&value))
        return std::min({(*triple)[0], (*triple)[1], (*triple)[2]});
    return 0.0;
}

double HighestOf(const Doc::ParamValue& value) {
    if (const auto* number = std::get_if<double>(&value)) return *number;
    if (const auto* whole = std::get_if<int>(&value)) return (double)*whole;
    if (const auto* pair = std::get_if<Doc::Vec2>(&value)) return std::max((*pair)[0], (*pair)[1]);
    if (const auto* triple = std::get_if<Doc::Vec3>(&value))
        return std::max({(*triple)[0], (*triple)[1], (*triple)[2]});
    return 0.0;
}

bool Bounded(const Doc::FieldDesc& field) {
    return field.range.max > field.range.min;
}

double Clamped(double value, const Doc::Range& range) {
    return std::clamp(value, (double)range.min, (double)range.max);
}

std::string ListText(const std::vector<std::string>& list) {
    if (list.empty()) return "(none)";
    std::string out;
    for (const std::string& item : list) {
        if (!out.empty()) out += ", ";
        out += item;
    }
    return out;
}

std::string StructuredText(const Doc::ParamValue& value) {
    if (const auto* list = std::get_if<std::vector<std::string>>(&value)) return ListText(*list);
    if (const auto* aspect = std::get_if<Doc::AspectSpec>(&value))
        return aspect->automatic ? "auto" : Number(aspect->value);
    if (const auto* clock = std::get_if<std::optional<Doc::ClipClock>>(&value)) {
        if (!clock->has_value()) return "auto";
        return std::string(Doc::kClipClockNames[(std::size_t)**clock]);
    }
    if (const auto* clip_time = std::get_if<Doc::ClipTime>(&value)) {
        if (clip_time->ticks.has_value()) return Number((double)*clip_time->ticks);
        return std::string(Doc::kClipClockNames[(std::size_t)clip_time->clock]);
    }
    if (const auto* orbit = std::get_if<std::optional<Doc::Orbit>>(&value))
        return orbit->has_value() ? "orbit" : "none";
    if (const auto* pulse = std::get_if<std::optional<Doc::PulseSpec>>(&value))
        return pulse->has_value() ? "pulse" : "none";
    if (const auto* scatter = std::get_if<std::optional<Doc::Scatter>>(&value))
        return scatter->has_value() ? "scatter" : "none";
    return "none";
}

std::string ValueTextOf(const Doc::FieldDesc& field, const Doc::ParamValue& value) {
    if (std::holds_alternative<std::monostate>(value)) return "none";
    if (const auto* flag = std::get_if<bool>(&value)) return *flag ? "on" : "off";
    if (const auto* whole = std::get_if<int>(&value)) {
        if (field.kind == Doc::FieldKind::Enum) return EnumText(field, *whole);
        return std::to_string(*whole);
    }
    if (const auto* number = std::get_if<double>(&value)) return Number(*number);
    if (const auto* text = std::get_if<std::string>(&value)) return text->empty() ? "-" : *text;
    if (const auto* pair = std::get_if<Doc::Vec2>(&value))
        return Number((*pair)[0]) + ", " + Number((*pair)[1]);
    if (const auto* triple = std::get_if<Doc::Vec3>(&value))
        return Number((*triple)[0]) + ", " + Number((*triple)[1]) + ", " + Number((*triple)[2]);
    return StructuredText(value);
}

}

std::vector<FormRow> FormRows(const Doc::Command& command) {
    const std::span<const Doc::FieldDesc> fields = Doc::FieldsFor(Doc::TypeOf(command));
    std::vector<FormRow> rows;
    rows.reserve(fields.size());
    const Doc::Command& defaults = Doc::DefaultCommand(Doc::TypeOf(command));
    for (const Doc::FieldDesc& field : fields) {
        rows.push_back(FormRow{.field = &field,
                               .at_default = AtCatalogDefault(command, field),
                               .default_text = ValueTextOf(field, field.get(defaults))});
    }
    return rows;
}

bool AtCatalogDefault(const Doc::Command& command, const Doc::FieldDesc& field) {
    return field.get(command) == field.get(Doc::DefaultCommand(Doc::TypeOf(command)));
}

Doc::ParamValue ClampField(const Doc::FieldDesc& field, Doc::ParamValue value) {
    if (!Bounded(field) || field.range.soft) return value;
    if (auto* number = std::get_if<double>(&value)) {
        *number = Clamped(*number, field.range);
        return value;
    }
    if (auto* whole = std::get_if<int>(&value)) {
        *whole = (int)std::lround(Clamped((double)*whole, field.range));
        return value;
    }
    if (auto* pair = std::get_if<Doc::Vec2>(&value)) {
        for (double& axis : *pair)
            axis = Clamped(axis, field.range);
        return value;
    }
    if (auto* triple = std::get_if<Doc::Vec3>(&value)) {
        for (double& axis : *triple)
            axis = Clamped(axis, field.range);
    }
    return value;
}

bool OutsideSoftRange(const Doc::FieldDesc& field, const Doc::ParamValue& value) {
    if (!Bounded(field) || !field.range.soft) return false;
    return LowestOf(value) < (double)field.range.min || HighestOf(value) > (double)field.range.max;
}

std::string DegreesText(const Doc::FieldDesc& field, const Doc::ParamValue& value) {
    if (!RadianField(field)) return {};
    if (const auto* number = std::get_if<double>(&value)) return Degrees(*number) + " deg";
    if (const auto* triple = std::get_if<Doc::Vec3>(&value)) {
        return Degrees((*triple)[0]) + " " + Degrees((*triple)[1]) + " " + Degrees((*triple)[2]) +
               " deg";
    }
    return {};
}

}
