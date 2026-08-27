#include "gui_gc2d_panel.h"
#include "gui_dpi.h"

#include "gc2d/gc_host.h"
#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <vector>

namespace Panels::Gc2dPanel {

namespace {

void DrawAnimationPicker(const Gc2dHost::Status& st) {
    const std::vector<std::string> names = Gc2dHost::ListAnimations();
    if (names.empty()) {
        ImGui::TextDisabled("This package declares no animations.");
        return;
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##gc2danim", st.animation.c_str())) {
        for (const auto& name : names) {
            const bool selected = (name == st.animation);
            if (ImGui::Selectable(name.c_str(), selected)) Gc2dHost::SelectAnimation(name);
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void DrawPlayback(const Gc2dHost::Status& st) {
    bool paused = st.paused;
    if (ImGui::Checkbox("Pause##gc2d", &paused)) Gc2dHost::SetPaused(paused);
    ImGui::SameLine();
    float speed = st.speed;
    ImGui::SetNextItemWidth(Gui::Dpi::S(120.0F));
    if (ImGui::SliderFloat("speed##gc2d", &speed, 0.0F, 4.0F, "%.2fx")) Gc2dHost::SetSpeed(speed);

    int frame = st.frame;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderInt("##gc2dframe", &frame, 0, std::max(st.length - 1, 0), "frame %d")) {
        Gc2dHost::SetPaused(true);
        Gc2dHost::SetFrame(frame);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Playhead in package frames. This animation runs %d frames.", st.length);
    }
}

}

void Render() {
    if (!Gc2dHost::Active()) {
        ImGui::TextDisabled("No 2D package loaded.");
        ImGui::TextWrapped("Pick a folder marked [2D package] in the Browse list. "
                           "Those are the system.idx / system.idr sprite packages.");
        return;
    }

    const Gc2dHost::Status st = Gc2dHost::GetStatus();
    ImGui::Text("%s", st.package.c_str());
    ImGui::TextDisabled("%d cells, %d records, %d animations, %d texture tiles", st.cells,
                        st.records, st.animations, st.tiles);
    ImGui::Separator();
    DrawAnimationPicker(st);
    DrawPlayback(st);
    ImGui::Separator();
    ImGui::TextDisabled("%d quads this frame", st.draw_nodes);
}

}
