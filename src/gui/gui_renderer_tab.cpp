#include <algorithm>

#include "gui_panels_internal.h"
#include "gui_live_controls.h"
#include "gui_layout_constants.h"
#include "gui_splitter.h"
#include "../state/app_state.h"
#include "imgui.h"

namespace Panels {

namespace {

void ClampPaneWidths(float avail_w, float sw, float& left_w, float& right_w, float& center_w) {
    left_w = std::max(left_w, Gui::kPaneLeftMin);
    const float pane_floor = Gui::kPaneRightMin;
    right_w = std::max(pane_floor, right_w);
    center_w = avail_w - left_w - right_w - (2.0F * sw);
    if (center_w < Gui::kPaneCenterMin) {
        float deficit = Gui::kPaneCenterMin - center_w;
        const float room_r = right_w - Gui::kPaneRightMin;
        const float take_r = room_r < deficit ? room_r : deficit;
        if (take_r > 0.0F) {
            right_w -= take_r;
            deficit -= take_r;
        }
        if (deficit > 0.0F) {
            const float room_l = left_w - Gui::kPaneLeftMin;
            const float take_l = room_l < deficit ? room_l : deficit;
            if (take_l > 0.0F) left_w -= take_l;
        }
        center_w = avail_w - left_w - right_w - (2.0F * sw);
        center_w = std::max(center_w, 1.0F);
    }
}

void RenderRightPane() {
    if (ImGui::CollapsingHeader("Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderLayersPanel();
    }
    {
        auto st = App::Global().GetStatus();
        const bool loaded = (st.stream_id != 0xFFFFFFFC);
        if (loaded) {
            ImGui::Spacing();
            LiveControls::RenderSeekControls();
        }
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    RenderVariantEditor();
    RenderAddSlotForm();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Labels", ImGuiTreeNodeFlags_DefaultOpen)) {
        LiveControls::RenderLabelsPanel();
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Sub-layers", ImGuiTreeNodeFlags_DefaultOpen)) {
        LiveControls::RenderSubLayersPanel();
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Live preview overrides", ImGuiTreeNodeFlags_DefaultOpen)) {
        LiveControls::RenderOverridePanel();
    }
}

}

void RenderRendererTabBody() {
    static float left_w = Gui::kPaneLeftDefault;
    static float right_w = Gui::kPaneRightDefault;

    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float row_h = ImGui::GetContentRegionAvail().y;
    const float sw = Gui::kSplitterW;

    float center_w = 0.0F;
    ClampPaneWidths(avail_w, sw, left_w, right_w, center_w);

    ImGui::BeginChild("pane_left", ImVec2(left_w, row_h), 0);
    RenderIfsPicker();
    ImGui::EndChild();

    ImGui::SameLine(0.0F, 0.0F);
    {
        float c = center_w;
        Gui::VSplitter("##split_l", sw, row_h, &left_w, &c, Gui::kPaneLeftMin, Gui::kPaneCenterMin);
    }
    ImGui::SameLine(0.0F, 0.0F);

    ImGui::BeginChild("pane_center", ImVec2(center_w, row_h), 0);
    RenderIfsInfoPane();
    ImGui::EndChild();

    ImGui::SameLine(0.0F, 0.0F);
    {
        float c = center_w;
        Gui::VSplitter("##split_r", sw, row_h, &c, &right_w, Gui::kPaneCenterMin,
                       Gui::kPaneRightMin);
    }
    ImGui::SameLine(0.0F, 0.0F);

    ImGui::BeginChild("pane_right", ImVec2(right_w, row_h), 0);
    RenderRightPane();
    ImGui::EndChild();
}

}
