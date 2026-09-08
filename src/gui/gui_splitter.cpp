#include "gui_splitter.h"
#include "gui_dpi.h"

#include "imgui.h"

namespace Gui {

bool VSplitter(const char* id, float width, float height, float* left_w, float* right_w,
               float left_min, float right_min) {
    ImGui::InvisibleButton(id, ImVec2(width, height));

    const bool active = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    if (active || hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

    ImGuiCol col_idx = ImGuiCol_Separator;
    if (active) {
        col_idx = ImGuiCol_SeparatorActive;
    } else if (hovered) {
        col_idx = ImGuiCol_SeparatorHovered;
    }
    const ImU32 col = ImGui::GetColorU32(col_idx);
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    const float cx = (p0.x + p1.x) * 0.5F;
    ImGui::GetWindowDrawList()->AddLine(ImVec2(cx, p0.y), ImVec2(cx, p1.y), col, Dpi::S(1.0F));

    if (active) {
        const float d = ImGui::GetIO().MouseDelta.x;
        if (d != 0.0F) {
            const float nl = *left_w + d;
            const float nr = *right_w - d;
            if (nl >= left_min && nr >= right_min) {
                *left_w = nl;
                *right_w = nr;
            }
        }
    }
    return active;
}

bool HSplitter(const char* id, float width, float height, float* top_h, float* bottom_h,
               float top_min, float bottom_min, float bottom_default) {
    ImGui::InvisibleButton(id, ImVec2(width, height));

    const bool active = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    if (active || hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

    ImGuiCol col_idx = ImGuiCol_Separator;
    if (active) {
        col_idx = ImGuiCol_SeparatorActive;
    } else if (hovered) {
        col_idx = ImGuiCol_SeparatorHovered;
    }
    const ImU32 col = ImGui::GetColorU32(col_idx);
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    const float cy = (p0.y + p1.y) * 0.5F;
    ImGui::GetWindowDrawList()->AddLine(ImVec2(p0.x, cy), ImVec2(p1.x, cy), col, Dpi::S(1.0F));

    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        const float total = *top_h + *bottom_h;
        *bottom_h = bottom_default;
        *top_h = total - bottom_default;
        return active;
    }

    if (active) {
        const float d = ImGui::GetIO().MouseDelta.y;
        if (d != 0.0F) {
            const float nt = *top_h + d;
            const float nb = *bottom_h - d;
            if (nt >= top_min && nb >= bottom_min) {
                *top_h = nt;
                *bottom_h = nb;
            }
        }
    }
    return active;
}

}
