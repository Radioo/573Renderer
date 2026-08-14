#include "gui_preset_workspace.h"

#include "gui_icons.h"
#include "gui_widgets.h"
#include "imgui.h"
#include "preset/preset_host.h"

#include <array>
#include <cctype>
#include <utility>
#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <string>
#include <vector>

namespace Panels::PresetWorkspace {

namespace {

char g_filter[96] = {};
std::string g_filter_scene;

bool Matches(const PresetHost::ParamView& param, const std::string& needle) {
    if (needle.empty()) return true;
    const auto has = [&needle](const std::string& hay) {
        return std::ranges::search(hay, needle, [](char a, char b) {
                   return std::tolower(a) == std::tolower(b);
               }).begin() != hay.end();
    };
    return has(param.label) || has(param.id) || has(param.group) || has(param.aliases);
}

void Tooltip(const PresetHost::ParamView& param) {
    if (!ImGui::IsItemHovered()) return;
    std::string text = param.id;
    if (!param.help.empty()) {
        text += "\n\n";
        text += param.help;
    }
    if (param.overridden) {
        text += "\n\nthe game authors this value; yours is different. Click the dot to revert.";
    }
    ImGui::SetTooltip("%s", text.c_str());
}

bool DrawValue(const PresetHost::ParamView& param, std::array<float, 3>& value, int& ivalue) {
    const std::string tag = "##pp" + param.id;
    ImGui::SetNextItemWidth(-FLT_MIN);
    const float speed = (param.step > 0.0F) ? param.step : 0.01F;
    switch (param.kind) {
    case 0:
        return ImGui::Checkbox(tag.c_str(), (bool*)&ivalue);
    case 1:
        if (param.soft || param.max <= param.min) return ImGui::DragInt(tag.c_str(), &ivalue, 1.0F);
        return ImGui::SliderInt(tag.c_str(), &ivalue, (int)param.min, (int)param.max);
    case 2: {
        std::vector<const char*> labels;
        labels.reserve(param.enum_labels.size());
        for (const std::string& label : param.enum_labels)
            labels.push_back(label.c_str());
        if (labels.empty()) return false;
        ivalue = std::clamp(ivalue, 0, (int)labels.size() - 1);
        return ImGui::Combo(tag.c_str(), &ivalue, labels.data(), (int)labels.size());
    }
    case 3:
        if (param.soft || param.max <= param.min)
            return ImGui::DragFloat(tag.c_str(), value.data(), speed, 0.0F, 0.0F, "%.4f");
        return ImGui::SliderFloat(tag.c_str(), value.data(), param.min, param.max, "%.4f");
    case 5:
        return ImGui::ColorEdit3(tag.c_str(), value.data(), ImGuiColorEditFlags_Float);
    default:
        return ImGui::DragFloat3(tag.c_str(), value.data(), speed, 0.0F, 0.0F, "%.4f");
    }
}

void DrawRow(const PresetHost::ParamView& param) {
    ImGui::PushID(param.id.c_str());
    ImGui::BeginGroup();

    ImGui::BeginDisabled(!param.overridden);
    if (ImGui::SmallButton(param.overridden ? "*" : ".")) PresetHost::ResetParam(param.id);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered() && param.overridden) {
        ImGui::SetTooltip("revert to the value the game authors");
    }
    ImGui::SameLine();

    std::string label = param.label;
    if (!param.unit.empty()) {
        label += "  ";
        label += param.unit;
    }
    ImGui::TextUnformatted(label.c_str());
    Tooltip(param);

    std::array<float, 3> value = param.value;
    int ivalue = param.ivalue;
    if (DrawValue(param, value, ivalue)) PresetHost::SetParam(param.id, value, ivalue);
    Tooltip(param);

    ImGui::EndGroup();
    ImGui::PopID();
}

void DrawGroup(const std::string& group, const std::vector<const PresetHost::ParamView*>& rows,
               bool force_open) {
    int changed = 0;
    for (const auto* row : rows) {
        if (row->overridden) changed++;
    }
    std::string header = group;
    if (changed > 0) {
        header += "  (";
        header += std::to_string(changed);
        header += " changed)";
    }
    if (force_open) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    const std::string tag = header + "###grp" + group;
    if (!ImGui::CollapsingHeader(tag.c_str())) return;

    ImGui::PushID(group.c_str());
    ImGui::BeginDisabled(changed == 0);
    if (ImGui::SmallButton("reset group")) PresetHost::ResetGroup(group);
    ImGui::EndDisabled();
    ImGui::PopID();

    for (const auto* row : rows)
        DrawRow(*row);
    ImGui::Spacing();
}

void DrawStates(const PresetHost::Status& status) {
    if (status.option_choices.empty()) return;
    const std::vector<PresetHost::StateView> states = PresetHost::ListStates();
    if (states.empty()) return;

    ImGui::TextDisabled("GAME STATES");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("What the screen's own code drives. Pick one to see it; the numbers "
                          "behind it are parameters in the groups below.");
    }
    for (size_t i = 0; i < states.size(); i++) {
        const PresetHost::StateView& state = states[i];
        ImGui::PushID((int)i);
        ImGui::TextUnformatted(state.label.c_str());
        for (size_t c = 0; c < state.choices.size(); c++) {
            if (c > 0 && (c % 4) != 0) ImGui::SameLine();
            const bool active = std::cmp_equal(c, state.choice);
            if (ImGui::RadioButton(state.choices[c].c_str(), active) && !active)
                PresetHost::SetOption((int)i, (int)c);
        }
        ImGui::PopID();
    }
    ImGui::Spacing();
    ImGui::Separator();
}

}

bool Visible() {
    return PresetHost::Active();
}

void Render() {
    const PresetHost::Status status = PresetHost::GetStatus();
    if (status.id != g_filter_scene) {
        g_filter_scene = status.id;
        g_filter[0] = '\0';
    }

    const int changed = PresetHost::ChangedParamCount();
    const std::string suffix = std::to_string(changed) + " changed";
    Gui::SectionHeader(ICON_SCENE, "Screen parameters", suffix.c_str());
    ImGui::BeginDisabled(changed == 0);
    if (ImGui::SmallButton("Reset all")) PresetHost::ResetAllParams();
    ImGui::EndDisabled();

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##paramfilter", "find parameter", g_filter, sizeof(g_filter));
    const std::string needle(g_filter);

    ImGui::BeginChild("param_body", ImVec2(0, 0), 0);
    if (needle.empty()) DrawStates(status);

    const std::vector<PresetHost::ParamView> params = PresetHost::ListParams();
    std::vector<std::string> groups;
    for (const auto& param : params) {
        if (!Matches(param, needle)) continue;
        if (std::ranges::find(groups, param.group) == groups.end()) groups.push_back(param.group);
    }
    int shown = 0;
    for (const std::string& group : groups) {
        std::vector<const PresetHost::ParamView*> rows;
        for (const auto& param : params) {
            if (param.group != group || !Matches(param, needle)) continue;
            rows.push_back(&param);
        }
        shown += (int)rows.size();
        DrawGroup(group, rows, !needle.empty());
    }
    if (!needle.empty()) ImGui::TextDisabled("%d matching parameter(s)", shown);
    if (params.empty()) ImGui::TextDisabled("This preset exposes no parameters.");
    ImGui::EndChild();
}

}
