#include "gui_tl_forms.h"

#include "editor/clip_form.h"
#include "gui/gui_dpi.h"
#include "imgui.h"
#include "preset/asset_index.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_fields.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

constexpr float kLabelWidthDips = 150.0F;
constexpr float kResetColumnDips = 30.0F;

float LabelWidth() {
    return Gui::Dpi::S(kLabelWidthDips);
}

float ResetColumn() {
    return Gui::Dpi::S(kResetColumnDips);
}

constexpr ImVec4 kNoteWarning(1.0F, 0.75F, 0.35F, 1.0F);

void DrawRowNote(ImGuiCol color, const char* text) {
    ImGui::Indent(LabelWidth());
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(color));
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::Unindent(LabelWidth());
}

FieldEvent Merge(FieldEvent a, FieldEvent b) {
    return (a > b) ? a : b;
}

FieldEvent Outcome(bool changed) {
    if (ImGui::IsItemDeactivatedAfterEdit()) return FieldEvent::Committed;
    return changed ? FieldEvent::Changed : FieldEvent::None;
}

std::string Placeholder(const FormContext& context, const Doc::FieldDesc& field) {
    if (field.id != "model" || context.target.empty()) return "(none)";
    return context.target + " (track target)";
}

std::vector<std::string> NamesFor(const FormContext& context, const std::string& field_id) {
    if (field_id == "asset") return context.asset_ids;
    const Preset::AssetEntry* entry = FindAsset(context, context.asset_id);
    if (entry == nullptr) return {};
    if (field_id == "model") return entry->models;
    if (field_id == "cell") return entry->cells;
    if (field_id != "animation") return {};
    std::vector<std::string> names;
    names.reserve(entry->animations.size());
    for (const Preset::AssetAnimation& animation : entry->animations)
        names.push_back(animation.name);
    return names;
}

FieldEvent DrawNameCombo(const char* id, std::string& value, const std::vector<std::string>& names,
                         bool loaded, const std::string& placeholder) {
    bool changed = false;
    ImGui::SetNextItemWidth(-ResetColumn());
    if (ImGui::BeginCombo(id, value.empty() ? placeholder.c_str() : value.c_str())) {
        for (const std::string& name : names) {
            const bool active = name == value;
            if (ImGui::Selectable(name.c_str(), active)) {
                value = name;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    if (!loaded || value.empty()) return changed ? FieldEvent::Committed : FieldEvent::None;
    if (std::ranges::find(names, value) == names.end()) RowWarning("not in the loaded asset");
    return changed ? FieldEvent::Committed : FieldEvent::None;
}

FieldEvent DrawStringRow(const char* id, std::string& value) {
    std::array<char, 128> buffer = {};
    const std::size_t copied = std::min(value.size(), buffer.size() - 1);
    std::copy_n(value.begin(), copied, buffer.begin());
    ImGui::SetNextItemWidth(-ResetColumn());
    const bool changed = ImGui::InputText(id, buffer.data(), buffer.size());
    if (changed) value = buffer.data();
    return Outcome(changed);
}

FieldEvent DrawStringList(const char* id, std::vector<std::string>& value) {
    std::string joined;
    for (const std::string& item : value) {
        if (!joined.empty()) joined += ", ";
        joined += item;
    }
    std::array<char, 256> buffer = {};
    const std::size_t copied = std::min(joined.size(), buffer.size() - 1);
    std::copy_n(joined.begin(), copied, buffer.begin());
    ImGui::SetNextItemWidth(-ResetColumn());
    const bool changed = ImGui::InputTextWithHint(id, "(none)", buffer.data(), buffer.size());
    if (!changed) return Outcome(false);
    value.clear();
    const std::string current(buffer.data());
    std::size_t at = 0;
    while (at <= current.size()) {
        const std::size_t comma = current.find(',', at);
        const std::string piece =
            current.substr(at, comma == std::string::npos ? comma : comma - at);
        const std::size_t first = piece.find_first_not_of(" \t");
        const std::size_t last = piece.find_last_not_of(" \t");
        if (first != std::string::npos) value.push_back(piece.substr(first, last - first + 1));
        if (comma == std::string::npos) break;
        at = comma + 1;
    }
    return Outcome(true);
}

FieldEvent DrawAspect(const char* id, Doc::AspectSpec& value) {
    bool automatic = value.automatic;
    bool changed = ImGui::Checkbox((std::string(id) + "_auto").c_str(), &automatic);
    if (changed) value.automatic = automatic;
    ImGui::SameLine();
    ImGui::TextUnformatted("auto");
    if (value.automatic) return changed ? FieldEvent::Committed : FieldEvent::None;
    ImGui::SameLine();
    auto number = (float)value.value;
    ImGui::SetNextItemWidth(-ResetColumn());
    if (ImGui::DragFloat(id, &number, 0.01F, 0.0F, 0.0F, "%.7f")) {
        value.value = number;
        changed = true;
    }
    return Merge(Outcome(changed), changed ? FieldEvent::Changed : FieldEvent::None);
}

FieldEvent DrawClock(const char* id, std::optional<Doc::ClipClock>& value) {
    int index = value.has_value() ? (int)*value + 1 : 0;
    const std::vector<std::string> names = {"auto", "continue", "restart"};
    const FieldEvent event = DrawEnumRow(id, nullptr, index, names);
    if (event == FieldEvent::None) return event;
    if (index <= 0) {
        value.reset();
        return event;
    }
    value = (Doc::ClipClock)(index - 1);
    return event;
}

FieldEvent DrawClipTime(const char* id, Doc::ClipTime& value) {
    int index = value.ticks.has_value() ? 2 : (int)value.clock;
    const std::vector<std::string> names = {"continue", "restart", "ticks"};
    const FieldEvent event = DrawEnumRow(id, nullptr, index, names);
    if (event != FieldEvent::None) {
        if (index < 2) {
            value.clock = (Doc::ClipClock)index;
            value.ticks.reset();
        } else if (!value.ticks.has_value()) {
            value.ticks = 0;
        }
    }
    if (!value.ticks.has_value()) return event;
    int ticks = *value.ticks;
    const FieldEvent number = DrawIntRow((std::string(id) + "_ticks").c_str(), "ticks", ticks);
    if (number != FieldEvent::None) value.ticks = ticks;
    return Merge(event, number);
}

template <class T>
FieldEvent DrawOptionalBlock(const char* id, std::optional<T>& value, const char* label) {
    bool present = value.has_value();
    const bool changed = ImGui::Checkbox(id, &present);
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
    if (!changed) return FieldEvent::None;
    if (present) {
        value = T{};
    } else {
        value.reset();
    }
    return FieldEvent::Committed;
}

FieldEvent DrawOrbit(const char* id, std::optional<Doc::Orbit>& value) {
    FieldEvent event = DrawOptionalBlock(id, value, "orbit path");
    if (!value.has_value()) return event;
    Doc::Orbit orbit = *value;
    event = Merge(event, DrawDoubleRow((std::string(id) + "_radius").c_str(), "radius",
                                       orbit.radius, 0.01F, ""));
    event = Merge(event, DrawDoubleRow((std::string(id) + "_rate").c_str(), "rate",
                                       orbit.rate_rad_per_frame, 0.001F, "rad/frame"));
    event = Merge(event, DrawDoubleRow((std::string(id) + "_zstart").c_str(), "z start",
                                       orbit.z_start, 0.01F, ""));
    event = Merge(event, DrawDoubleRow((std::string(id) + "_zper").c_str(), "z per frame",
                                       orbit.z_per_frame, 0.001F, ""));
    event = Merge(
        event, DrawDoubleRow((std::string(id) + "_zmin").c_str(), "z min", orbit.z_min, 0.01F, ""));
    if (event != FieldEvent::None) value = orbit;
    return event;
}

FieldEvent DrawPulse(const char* id, std::optional<Doc::PulseSpec>& value) {
    FieldEvent event = DrawOptionalBlock(id, value, "beat pulse");
    if (!value.has_value()) return event;
    Doc::PulseSpec pulse = *value;
    int grid = (int)pulse.grid;
    event =
        Merge(event, DrawEnumRow((std::string(id) + "_grid").c_str(), "grid", grid, {"a", "b"}));
    pulse.grid = (Doc::Grid)grid;
    event = Merge(event, DrawDoubleRow((std::string(id) + "_odd").c_str(), "scale odd",
                                       pulse.scale_odd, 0.005F, ""));
    event = Merge(event, DrawDoubleRow((std::string(id) + "_even").c_str(), "scale even",
                                       pulse.scale_even, 0.005F, ""));
    event = Merge(event, DrawIntRow((std::string(id) + "_frames").c_str(), "frames", pulse.frames));
    if (event != FieldEvent::None) value = pulse;
    return event;
}

FieldEvent DrawScatter(const char* id, std::optional<Doc::Scatter>& value) {
    FieldEvent event = DrawOptionalBlock(id, value, "scatter box");
    if (!value.has_value()) return event;
    Doc::Scatter scatter = *value;
    auto span = ImVec2((float)scatter.span[0], (float)scatter.span[1]);
    auto offset = ImVec2((float)scatter.offset[0], (float)scatter.offset[1]);
    RowLabel("span");
    ImGui::SetNextItemWidth(-ResetColumn());
    if (ImGui::DragFloat2((std::string(id) + "_span").c_str(), &span.x, 1.0F)) {
        scatter.span = {span.x, span.y};
        event = Merge(event, FieldEvent::Changed);
    }
    event = Merge(event, Outcome(false));
    RowLabel("offset");
    ImGui::SetNextItemWidth(-ResetColumn());
    if (ImGui::DragFloat2((std::string(id) + "_offset").c_str(), &offset.x, 1.0F)) {
        scatter.offset = {offset.x, offset.y};
        event = Merge(event, FieldEvent::Changed);
    }
    event = Merge(event, Outcome(false));
    if (event != FieldEvent::None) value = scatter;
    return event;
}

FieldEvent DrawBurst(const char* id, std::optional<Doc::Burst>& value) {
    FieldEvent event = DrawOptionalBlock(id, value, "rising burst");
    if (!value.has_value()) return event;
    Doc::Burst burst = *value;
    const std::array<std::pair<const char*, int*>, 9> rows = {
        std::pair{"period base", &burst.period_base},
        std::pair{"period span", &burst.period_span},
        std::pair{"life drift", &burst.life_drift},
        std::pair{"rise base", &burst.rise_base},
        std::pair{"rise step", &burst.rise_step},
        std::pair{"rise period", &burst.rise_period},
        std::pair{"span x", &burst.span_x},
        std::pair{"from y", &burst.from_y},
        std::pair{"to y", &burst.to_y}};
    for (const auto& [label, field] : rows)
        event = Merge(event, DrawIntRow((std::string(id) + "_" + label).c_str(), label, *field));
    if (event != FieldEvent::None) value = burst;
    return event;
}

FieldEvent DrawMovie(const char* id, std::optional<Doc::MovieTexture>& value) {
    FieldEvent event = DrawOptionalBlock(id, value, "movie file");
    if (!value.has_value()) return event;
    Doc::MovieTexture movie = *value;
    RowLabel("movie");
    event = Merge(event, DrawStringRow((std::string(id) + "_path").c_str(), movie.path));
    if (event != FieldEvent::None) value = movie;
    return event;
}

FieldEvent DrawOverride(const char* id, Doc::OverrideValue& value) {
    if (auto* number = std::get_if<double>(&value)) {
        return DrawDoubleRow(id, nullptr, *number, 0.01F, "");
    }
    if (auto* flag = std::get_if<bool>(&value)) {
        const bool changed = ImGui::Checkbox(id, flag);
        return changed ? FieldEvent::Committed : FieldEvent::None;
    }
    if (auto* triple = std::get_if<Doc::Vec3>(&value)) {
        return DrawVec3Row(id, nullptr, *triple, 0.01F, "");
    }
    return DrawStringRow(id, std::get<std::string>(value));
}

Doc::ParamValue Materialize(Doc::FieldKind kind) {
    switch (kind) {
    case Doc::FieldKind::Bool:
        return false;
    case Doc::FieldKind::Int:
    case Doc::FieldKind::Enum:
        return 0;
    case Doc::FieldKind::Float:
        return 0.0;
    case Doc::FieldKind::String:
        return std::string{};
    case Doc::FieldKind::Vec2:
        return Doc::Vec2{};
    case Doc::FieldKind::Vec3:
        return Doc::Vec3{};
    case Doc::FieldKind::Aspect:
        return Doc::AspectSpec{};
    default:
        return {};
    }
}

std::vector<std::string> EnumNames(const Doc::FieldDesc& field) {
    std::vector<std::string> names;
    names.reserve(field.enum_names.size());
    for (const std::string_view name : field.enum_names)
        names.emplace_back(name);
    return names;
}

}

void RowNote(const char* text) {
    DrawRowNote(ImGuiCol_TextDisabled, text);
}

void RowWarning(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, kNoteWarning);
    DrawRowNote(ImGuiCol_TextDisabled, text);
    ImGui::PopStyleColor();
}

void RowLabel(const char* label) {
    if (label == nullptr || label[0] == '\0') return;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(0.0F, std::max(Gui::Dpi::S(6.0F), LabelWidth() - ImGui::CalcTextSize(label).x));
}

const Preset::AssetEntry* FindAsset(const FormContext& context, const std::string& asset_id) {
    if (context.assets == nullptr) return nullptr;
    for (const Preset::AssetEntry& entry : context.assets->assets) {
        if (entry.id == asset_id) return &entry;
    }
    return nullptr;
}

FieldEvent DrawVec3Row(const char* id, const char* label, Doc::Vec3& value, float speed,
                       const char* unit) {
    const bool labelled = label != nullptr && label[0] != '\0';
    if (labelled && unit != nullptr && unit[0] != '\0') {
        RowLabel((std::string(label) + "  " + unit).c_str());
    } else {
        RowLabel(label);
    }
    const std::array<const char*, 3> axes = {"_x", "_y", "_z"};
    const float width = (ImGui::GetContentRegionAvail().x - ResetColumn() -
                         (2.0F * ImGui::GetStyle().ItemInnerSpacing.x)) /
                        3.0F;
    FieldEvent event = FieldEvent::None;
    for (std::size_t i = 0; i < axes.size(); i++) {
        if (i > 0) ImGui::SameLine(0.0F, ImGui::GetStyle().ItemInnerSpacing.x);
        auto number = (float)value[i];
        ImGui::SetNextItemWidth(std::max(Gui::Dpi::S(48.0F), width));
        const bool changed = ImGui::DragFloat((std::string(id) + axes[i]).c_str(), &number, speed,
                                              0.0F, 0.0F, "%.4f");
        if (changed) value[i] = number;
        event = Merge(event, Outcome(changed));
    }
    return event;
}

FieldEvent DrawDoubleRow(const char* id, const char* label, double& value, float speed,
                         const char* unit) {
    const bool labelled = label != nullptr && label[0] != '\0';
    if (labelled && unit != nullptr && unit[0] != '\0') {
        RowLabel((std::string(label) + "  " + unit).c_str());
    } else {
        RowLabel(label);
    }
    auto number = (float)value;
    ImGui::SetNextItemWidth(-ResetColumn());
    const bool changed = ImGui::DragFloat(id, &number, speed, 0.0F, 0.0F, "%.4f");
    if (changed) value = number;
    return Outcome(changed);
}

FieldEvent DrawIntRow(const char* id, const char* label, int& value) {
    RowLabel(label);
    ImGui::SetNextItemWidth(-ResetColumn());
    const bool changed = ImGui::DragInt(id, &value, 1.0F);
    return Outcome(changed);
}

FieldEvent DrawEnumRow(const char* id, const char* label, int& value,
                       const std::vector<std::string>& names) {
    RowLabel(label);
    if (names.empty()) return FieldEvent::None;
    value = std::clamp(value, 0, (int)names.size() - 1);
    bool changed = false;
    ImGui::SetNextItemWidth(-ResetColumn());
    if (ImGui::BeginCombo(id, names[(std::size_t)value].c_str())) {
        for (std::size_t i = 0; i < names.size(); i++) {
            const bool active = std::cmp_equal(i, value);
            if (ImGui::Selectable(names[i].c_str(), active)) {
                value = (int)i;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed ? FieldEvent::Committed : FieldEvent::None;
}

namespace {

FieldEvent DrawVec2Row(const char* id, const char* label, Doc::Vec2& value, float speed,
                       const char* unit) {
    const bool labelled = label != nullptr && label[0] != '\0';
    if (labelled && unit != nullptr && unit[0] != '\0') {
        RowLabel((std::string(label) + "  " + unit).c_str());
    } else {
        RowLabel(label);
    }
    const std::array<const char*, 2> axes = {"_x", "_y"};
    const float width =
        (ImGui::GetContentRegionAvail().x - ResetColumn() - ImGui::GetStyle().ItemInnerSpacing.x) /
        2.0F;
    FieldEvent event = FieldEvent::None;
    for (std::size_t i = 0; i < axes.size(); i++) {
        if (i > 0) ImGui::SameLine(0.0F, ImGui::GetStyle().ItemInnerSpacing.x);
        auto number = (float)value[i];
        ImGui::SetNextItemWidth(std::max(Gui::Dpi::S(48.0F), width));
        const bool changed = ImGui::DragFloat((std::string(id) + axes[i]).c_str(), &number, speed,
                                              0.0F, 0.0F, "%.4f");
        if (changed) value[i] = number;
        event = Merge(event, Outcome(changed));
    }
    return event;
}

bool DrawScalar(const FormContext& context, const Doc::FieldDesc& field, const std::string& id,
                Doc::ParamValue& value, FieldEvent& event) {
    if (auto* text = std::get_if<std::string>(&value)) {
        const std::vector<std::string> names = NamesFor(context, std::string(field.id));
        RowLabel(std::string(field.id).c_str());
        if (!names.empty()) {
            const Preset::AssetEntry* entry = FindAsset(context, context.asset_id);
            event = DrawNameCombo(id.c_str(), *text, names, entry != nullptr && entry->loaded,
                                  Placeholder(context, field));
        } else {
            event = DrawStringRow(id.c_str(), *text);
        }
    } else if (auto* flag = std::get_if<bool>(&value)) {
        RowLabel(std::string(field.id).c_str());
        if (ImGui::Checkbox(id.c_str(), flag)) event = FieldEvent::Committed;
    } else if (auto* whole = std::get_if<int>(&value)) {
        if (field.kind == Doc::FieldKind::Enum) {
            event =
                DrawEnumRow(id.c_str(), std::string(field.id).c_str(), *whole, EnumNames(field));
        } else {
            event = DrawIntRow(id.c_str(), std::string(field.id).c_str(), *whole);
        }
    } else if (auto* number = std::get_if<double>(&value)) {
        const float speed = field.range.step > 0.0F ? field.range.step : 0.01F;
        event = DrawDoubleRow(id.c_str(), std::string(field.id).c_str(), *number, speed,
                              std::string(field.unit).c_str());
    } else if (auto* pair = std::get_if<Doc::Vec2>(&value)) {
        const float speed = field.range.step > 0.0F ? field.range.step : 1.0F;
        event = DrawVec2Row(id.c_str(), std::string(field.id).c_str(), *pair, speed,
                            std::string(field.unit).c_str());
    } else if (auto* triple = std::get_if<Doc::Vec3>(&value)) {
        const float speed = field.range.step > 0.0F ? field.range.step : 0.01F;
        event = DrawVec3Row(id.c_str(), std::string(field.id).c_str(), *triple, speed,
                            std::string(field.unit).c_str());
    } else {
        return false;
    }
    return true;
}

bool DrawStructured(const Doc::FieldDesc& field, const std::string& id, Doc::ParamValue& value,
                    FieldEvent& event) {
    if (auto* list = std::get_if<std::vector<std::string>>(&value)) {
        RowLabel(std::string(field.id).c_str());
        event = DrawStringList(id.c_str(), *list);
    } else if (auto* aspect = std::get_if<Doc::AspectSpec>(&value)) {
        RowLabel(std::string(field.id).c_str());
        event = DrawAspect(id.c_str(), *aspect);
    } else if (auto* clock = std::get_if<std::optional<Doc::ClipClock>>(&value)) {
        RowLabel(std::string(field.id).c_str());
        event = DrawClock(id.c_str(), *clock);
    } else if (auto* clip_time = std::get_if<Doc::ClipTime>(&value)) {
        RowLabel(std::string(field.id).c_str());
        event = DrawClipTime(id.c_str(), *clip_time);
    } else if (auto* orbit = std::get_if<std::optional<Doc::Orbit>>(&value)) {
        RowLabel(std::string(field.id).c_str());
        event = DrawOrbit(id.c_str(), *orbit);
    } else if (auto* pulse = std::get_if<std::optional<Doc::PulseSpec>>(&value)) {
        RowLabel(std::string(field.id).c_str());
        event = DrawPulse(id.c_str(), *pulse);
    } else if (auto* scatter = std::get_if<std::optional<Doc::Scatter>>(&value)) {
        RowLabel(std::string(field.id).c_str());
        event = DrawScatter(id.c_str(), *scatter);
    } else if (auto* burst = std::get_if<std::optional<Doc::Burst>>(&value)) {
        RowLabel(std::string(field.id).c_str());
        event = DrawBurst(id.c_str(), *burst);
    } else if (auto* movie = std::get_if<std::optional<Doc::MovieTexture>>(&value)) {
        RowLabel(std::string(field.id).c_str());
        event = DrawMovie(id.c_str(), *movie);
    } else {
        return false;
    }
    return true;
}

FieldEvent DrawOverrideField(const Doc::FieldDesc& field, const std::string& id,
                             Doc::Command& command) {
    RowLabel(std::string(field.id).c_str());
    Doc::OverrideValue held;
    if (const auto* current = std::get_if<Doc::ParamOverrideCmd>(&command)) held = current->value;
    const FieldEvent event = DrawOverride(id.c_str(), held);
    if (event != FieldEvent::None && std::holds_alternative<Doc::ParamOverrideCmd>(command)) {
        std::get<Doc::ParamOverrideCmd>(command).value = held;
    }
    return event;
}

FieldEvent DrawInherited(const Doc::FieldDesc& field, const std::string& id,
                         Doc::Command& command) {
    RowLabel(std::string(field.id).c_str());
    bool present = false;
    const bool set = ImGui::Checkbox(id.c_str(), &present);
    ImGui::SameLine();
    ImGui::TextDisabled("inherits the document value");
    if (!set) return FieldEvent::None;
    field.set(command, Materialize(field.kind));
    return FieldEvent::Committed;
}

}

FieldEvent DrawField(const FormContext& context, const Doc::FieldDesc& field,
                     Doc::Command& command) {
    const std::string id = "###tl_field_" + std::string(field.id);
    Doc::ParamValue value = field.get(command);
    FieldEvent event = FieldEvent::None;

    if (std::holds_alternative<std::monostate>(value)) return DrawInherited(field, id, command);

    ImGui::BeginGroup();
    const bool drawn =
        DrawScalar(context, field, id, value, event) || DrawStructured(field, id, value, event);
    if (!drawn) event = DrawOverrideField(field, id, command);
    ImGui::EndGroup();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        const std::string unit =
            field.unit.empty() ? std::string{} : "\nunit: " + std::string(field.unit);
        ImGui::SetTooltip("%s%s\n%s%s", std::string(field.id).c_str(), unit.c_str(),
                          std::string(field.help).c_str(),
                          field.tweenable ? "\ntweenable: allowed in a key" : "");
    }
    if (!drawn || event == FieldEvent::None) return event;
    field.set(command, Editor::ClampField(field, value));
    return event;
}

FieldEvent DrawParamForm(const FormContext& context, Doc::Command& command) {
    FieldEvent event = FieldEvent::None;
    for (const Editor::FormRow& row : Editor::FormRows(command)) {
        const Doc::FieldDesc& field = *row.field;
        event = Merge(event, DrawField(context, field, command));
        if (!Editor::AtCatalogDefault(command, field)) {
            const std::string reset = "R###tl_reset_" + std::string(field.id);
            ImGui::SameLine(0.0F, Gui::Dpi::S(4.0F));
            if (ImGui::SmallButton(reset.c_str())) {
                field.set(command, field.get(Doc::DefaultCommand(Doc::TypeOf(command))));
                event = FieldEvent::Committed;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("reset to the catalog default: %s", row.default_text.c_str());
            }
        }
        const Doc::ParamValue current = field.get(command);
        const std::string degrees = Editor::DegreesText(field, current);
        if (!degrees.empty()) RowNote(degrees.c_str());
        if (Editor::OutsideSoftRange(field, current)) {
            std::array<char, 96> range = {};
            snprintf(range.data(), range.size(), "outside the usual range %.3f..%.3f",
                     (double)field.range.min, (double)field.range.max);
            RowWarning(range.data());
        }
    }
    return event;
}

}
