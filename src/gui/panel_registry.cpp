#include "panel_registry.h"

#include "../state/app_state.h"
#include "gui_panels_internal.h"
#include "gc2d/gc_host.h"
#include "gui_gc2d_panel.h"
#include "gui_scene3d_panel.h"
#include "scene3d/scene3d_host.h"

#include <span>
#include <string>
#include <vector>

namespace Gui {

namespace {

bool QproTabVisible() {
    return App::Global().GetGameProfileSlug() == "iidx33";
}

bool Scene3dTabVisible() {
    return Scene3dHost::Active();
}

bool Gc2dTabVisible() {
    return Gc2dHost::Active();
}

constexpr PanelDesc kModernPanels[] = {
    {.id = "renderer_view",
     .tab_label = "Renderer",
     .slot = PanelSlot::MainTab,
     .draw = &Panels::RenderRendererView,
     .visible = nullptr},
    {.id = "qpro_view",
     .tab_label = "qpro",
     .slot = PanelSlot::MainTab,
     .draw = &Panels::RenderQproTabBody,
     .visible = &QproTabVisible},
    {.id = "properties",
     .tab_label = "Properties",
     .slot = PanelSlot::InspectorTab,
     .draw = &Panels::RenderPropertiesTab,
     .visible = nullptr},
    {.id = "render",
     .tab_label = "Render",
     .slot = PanelSlot::InspectorTab,
     .draw = &Panels::RenderRenderTabModern,
     .visible = nullptr},
    {.id = "live",
     .tab_label = "Live",
     .slot = PanelSlot::InspectorTab,
     .draw = &Panels::RenderLiveTab,
     .visible = nullptr},
};

constexpr PanelDesc kDdrPanels[] = {
    {.id = "renderer_view",
     .tab_label = "Renderer",
     .slot = PanelSlot::MainTab,
     .draw = &Panels::RenderRendererView,
     .visible = nullptr},
    {.id = "render",
     .tab_label = "Render",
     .slot = PanelSlot::InspectorTab,
     .draw = &Panels::RenderRenderTabDdr,
     .visible = nullptr},
    {.id = "live",
     .tab_label = "Live",
     .slot = PanelSlot::InspectorTab,
     .draw = &Panels::RenderLiveTab,
     .visible = nullptr},
    {.id = "scene3d",
     .tab_label = "3D scene",
     .slot = PanelSlot::InspectorTab,
     .draw = &Panels::Scene3dPanel::Render,
     .visible = &Scene3dTabVisible},
};

constexpr PanelDesc kScene3dPanels[] = {
    {.id = "renderer_view",
     .tab_label = "Renderer",
     .slot = PanelSlot::MainTab,
     .draw = &Panels::RenderRendererView,
     .visible = nullptr},
    {.id = "scene3d",
     .tab_label = "3D scene",
     .slot = PanelSlot::InspectorTab,
     .draw = &Panels::Scene3dPanel::Render,
     .visible = &Scene3dTabVisible},
    {.id = "gc2d",
     .tab_label = "2D package",
     .slot = PanelSlot::InspectorTab,
     .draw = &Panels::Gc2dPanel::Render,
     .visible = &Gc2dTabVisible},
};

struct PanelSet {
    const char* backend_id = nullptr;
    std::span<const PanelDesc> panels;
};

constexpr PanelSet kPanelSets[] = {
    {.backend_id = "afp_modern", .panels = kModernPanels},
    {.backend_id = "afp_ddr", .panels = kDdrPanels},
    {.backend_id = "scene3d", .panels = kScene3dPanels},
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
