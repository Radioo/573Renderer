#include "panel_registry.h"

#include "../state/app_state.h"
#include "../state/telemetry.h"
#include "gui_live_controls.h"
#include "gui_panels_internal.h"
#include "imgui.h"

#include <span>
#include <string>
#include <vector>

namespace Gui {

namespace {

void LeadingSeparator() {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void SectionLayersFull() {
    if (ImGui::CollapsingHeader("Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
        Panels::RenderLayersPanel();
    }
}

void SectionLayersList() {
    if (ImGui::CollapsingHeader("Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
        Panels::RenderLayersListOnly();
    }
}

void SectionSeek() {
    auto st = App::Global().GetStatus();
    if (st.scene_loaded) {
        ImGui::Spacing();
        Panels::LiveControls::RenderSeekControls();
    }
}

void SectionVariants() {
    LeadingSeparator();
    Panels::RenderVariantEditor();
    Panels::RenderAddSlotForm();
}

void SectionLabels() {
    LeadingSeparator();
    if (ImGui::CollapsingHeader("Labels", ImGuiTreeNodeFlags_DefaultOpen)) {
        Panels::LiveControls::RenderLabelsPanel();
    }
}

void SectionSubLayers() {
    LeadingSeparator();
    if (ImGui::CollapsingHeader("Sub-layers", ImGuiTreeNodeFlags_DefaultOpen)) {
        Panels::LiveControls::RenderSubLayersPanel();
    }
}

void SectionOverrides() {
    LeadingSeparator();
    if (ImGui::CollapsingHeader("Live preview overrides", ImGuiTreeNodeFlags_DefaultOpen)) {
        Panels::LiveControls::RenderOverridePanel();
    }
}

bool QproTabVisible() {
    return App::Global().GetGameProfileSlug() == "iidx33";
}

constexpr PanelDesc kModernPanels[] = {
    {.id = "renderer_tab",
     .tab_label = "Renderer",
     .slot = PanelSlot::MainTab,
     .draw = &Panels::RenderRendererTabBody,
     .visible = nullptr},
    {.id = "qpro_tab",
     .tab_label = "qpro",
     .slot = PanelSlot::MainTab,
     .draw = &Panels::RenderQproTabBody,
     .visible = &QproTabVisible},
    {.id = "layers",
     .tab_label = nullptr,
     .slot = PanelSlot::RightStack,
     .draw = &SectionLayersFull,
     .visible = nullptr},
    {.id = "seek",
     .tab_label = nullptr,
     .slot = PanelSlot::RightStack,
     .draw = &SectionSeek,
     .visible = nullptr},
    {.id = "variants",
     .tab_label = nullptr,
     .slot = PanelSlot::RightStack,
     .draw = &SectionVariants,
     .visible = nullptr},
    {.id = "labels",
     .tab_label = nullptr,
     .slot = PanelSlot::RightStack,
     .draw = &SectionLabels,
     .visible = nullptr},
    {.id = "sublayers",
     .tab_label = nullptr,
     .slot = PanelSlot::RightStack,
     .draw = &SectionSubLayers,
     .visible = nullptr},
    {.id = "overrides",
     .tab_label = nullptr,
     .slot = PanelSlot::RightStack,
     .draw = &SectionOverrides,
     .visible = nullptr},
};

constexpr PanelDesc kDdrPanels[] = {
    {.id = "renderer_tab",
     .tab_label = "Renderer",
     .slot = PanelSlot::MainTab,
     .draw = &Panels::RenderRendererTabBody,
     .visible = nullptr},
    {.id = "layers",
     .tab_label = nullptr,
     .slot = PanelSlot::RightStack,
     .draw = &SectionLayersList,
     .visible = nullptr},
    {.id = "seek",
     .tab_label = nullptr,
     .slot = PanelSlot::RightStack,
     .draw = &SectionSeek,
     .visible = nullptr},
    {.id = "labels",
     .tab_label = nullptr,
     .slot = PanelSlot::RightStack,
     .draw = &SectionLabels,
     .visible = nullptr},
    {.id = "overrides",
     .tab_label = nullptr,
     .slot = PanelSlot::RightStack,
     .draw = &SectionOverrides,
     .visible = nullptr},
};

struct PanelSet {
    const char* backend_id = nullptr;
    std::span<const PanelDesc> panels;
};

constexpr PanelSet kPanelSets[] = {
    {.backend_id = "afp_modern", .panels = kModernPanels},
    {.backend_id = "afp_ddr", .panels = kDdrPanels},
};

}

void CollectActivePanels(PanelSlot slot, std::vector<const PanelDesc*>& out) {
    out.clear();
    const std::string backend_id = App::Global().ActiveBackendId();
    for (const auto& set : kPanelSets) {
        if (backend_id != set.backend_id) continue;
        for (const auto& p : set.panels) {
            if (p.slot != slot) continue;
            if (p.visible != nullptr && !p.visible()) continue;
            out.push_back(&p);
        }
        break;
    }
}

}
