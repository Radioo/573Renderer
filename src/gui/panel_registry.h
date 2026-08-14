#pragma once

#include <vector>

namespace Gui {

enum class PanelSlot : int {
    MainTab,
    InspectorTab,
    CenterPane,
};

struct PanelDesc {
    const char* id;
    const char* tab_label;
    PanelSlot slot;
    void (*draw)();
    bool (*visible)();
};

void CollectActivePanels(PanelSlot slot, std::vector<const PanelDesc*>& out);

}
