#include "gui_widgets.h"
#include "gui_style.h"
#include "gui_dpi.h"

#include "imgui.h"

namespace Gui {

bool Segmented(const char* id, const char* const* items, int count, int* current) {
    bool changed = false;
    ImGui::PushID(id);
    const ImGuiStyle& st = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Dpi::S(1.0F), st.ItemSpacing.y));
    for (int i = 0; i < count; i++) {
        const float item_w = ImGui::CalcTextSize(items[i]).x + (st.FramePadding.x * 2.0F);
        if (i > 0) {
            ImGui::SameLine();
            if (ImGui::GetContentRegionAvail().x < item_w) ImGui::NewLine();
        }
        const bool active = (i == *current);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        }
        ImGui::PushID(i);
        if (ImGui::Button(items[i]) && !active) {
            *current = i;
            changed = true;
        }
        ImGui::PopID();
        ImGui::PopStyleColor(2);
    }
    ImGui::PopStyleVar();
    ImGui::PopID();
    return changed;
}

void SectionHeader(const char* icon, const char* label, const char* suffix) {
    if (icon != nullptr) {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", icon);
        ImGui::SameLine(0.0F, Dpi::S(6.0F));
    }
    PushHeaderFont();
    ImGui::TextUnformatted(label);
    ImGui::PopFont();
    if (suffix != nullptr && suffix[0] != '\0') {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", suffix);
    }
    ImGui::Separator();
    ImGui::Spacing();
}

}
