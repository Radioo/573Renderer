#include "gui_icons.h"
#include "gui_dpi.h"
#include "gui_panels_internal.h"
#include "gui_widgets.h"
#include "../backend/afp_commands.h"
#include "../state/app_state.h"
#include "imgui.h"
#include "state/ifs_catalog.h"
#include "state/telemetry.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace Panels {

namespace Scene {

namespace {
Selection g_selection;
}

const Selection& Current() {
    return g_selection;
}

void Select(Selection s) {
    g_selection = std::move(s);
}

void Reset() {
    g_selection = Selection{};
}

}

namespace {

std::string Lower(const std::string& in) {
    std::string s = in;
    for (auto& c : s)
        c = (char)tolower((unsigned char)c);
    return s;
}

bool SubtreeMatches(const App::SubLayerNode& node, const std::string& lower_filter) {
    if (lower_filter.empty()) return true;
    if (Lower(node.name).find(lower_filter) != std::string::npos) return true;
    return std::ranges::any_of(node.children,
                               [&](const auto& c) { return SubtreeMatches(c, lower_filter); });
}

const App::Status::McChild* FindChildPos(const App::Status& status, const std::string& name) {
    for (const auto& c : status.mc_children) {
        if (c.name == name && c.have_pos) return &c;
    }
    return nullptr;
}

const App::VariantSlot* FindSlot(const App::IfsConfig& cfg, const App::SubLayerNode& node) {
    for (const auto& s : cfg.slots) {
        if (s.path == node.path || s.path == node.name) return &s;
    }
    return nullptr;
}

bool NodeMatchesPath(const App::SubLayerNode& node, const std::string& path) {
    if (node.path == path) return true;
    return std::ranges::any_of(node.children,
                               [&](const auto& c) { return NodeMatchesPath(c, path); });
}

void DrawVariantBadge() {
    ImGui::SameLine();
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "variant");
}

void RenderSceneNode(App::State& state, const std::string& active, const App::Status& status,
                     const App::IfsConfig& cfg, const App::SubLayerNode& node,
                     const std::vector<std::pair<std::string, bool>>& overrides,
                     const std::string& lower_filter, int idx) {
    if (!SubtreeMatches(node, lower_filter)) return;

    bool visible = true;
    for (const auto& ov : overrides) {
        if (ov.first == node.path) {
            visible = ov.second;
            break;
        }
    }

    ImGui::PushID(idx);
    if (ImGui::Checkbox("##vis", &visible)) state.SetSublayerOverride(active, node.path, visible);
    ImGui::SameLine();

    const bool is_leaf = node.enumerated && node.children.empty();
    const bool selected = (Scene::Current().kind == Scene::Selection::Kind::Child &&
                           Scene::Current().path == node.path);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (is_leaf) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (selected) flags |= ImGuiTreeNodeFlags_Selected;
    if (!lower_filter.empty()) flags |= ImGuiTreeNodeFlags_DefaultOpen;

    const bool open = ImGui::TreeNodeEx(node.name.c_str(), flags);
    const bool toggled_open = ImGui::IsItemToggledOpen();
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !toggled_open) {
        Scene::Select(
            {.kind = Scene::Selection::Kind::Child, .path = node.path, .name = node.name});
    }

    const App::Status::McChild* pos = FindChildPos(status, node.name);
    if (pos != nullptr) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%.0f, %.0f)", pos->x, pos->y);
    }
    const App::VariantSlot* slot = FindSlot(cfg, node);
    if (slot != nullptr && slot->is_valid) DrawVariantBadge();

    if (!is_leaf) {
        if (toggled_open) state.SetSublayerExpanded(node.path, open);
        if (open) {
            int ci = 0;
            for (const auto& c : node.children)
                RenderSceneNode(state, active, status, cfg, c, overrides, lower_filter, ci++);
            ImGui::TreePop();
        }
    }
    ImGui::PopID();
}

void RenderUnresolvedSlots(const App::Status& status, const App::IfsConfig& cfg) {
    for (const auto& slot : cfg.slots) {
        if (slot.is_valid && NodeMatchesPath(status.mc_tree, slot.path)) continue;
        ImGui::PushID(slot.path.c_str());
        const bool selected = (Scene::Current().kind == Scene::Selection::Kind::Child &&
                               Scene::Current().path == slot.path);
        char row[192];
        snprintf(row, sizeof(row), "   %s", slot.path.c_str());
        if (ImGui::Selectable(row, selected)) {
            Scene::Select(
                {.kind = Scene::Selection::Kind::Child, .path = slot.path, .name = slot.path});
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", slot.is_valid ? "(slot)" : "(unresolved slot)");
        ImGui::PopID();
    }
}

void RenderLayerRow(App::State& state, const std::string& active, const App::Status& status,
                    const App::IfsConfig& cfg,
                    const std::vector<std::pair<std::string, bool>>& overrides,
                    const std::string& lower_filter, const std::string& name, int idx) {
    const bool is_playing = (name == status.playing_animation);
    const bool has_children = is_playing && !status.mc_tree.children.empty();
    if (!lower_filter.empty() && Lower(name).find(lower_filter) == std::string::npos &&
        !(has_children && SubtreeMatches(status.mc_tree, lower_filter))) {
        return;
    }

    const bool selected =
        (Scene::Current().kind == Scene::Selection::Kind::Layer && Scene::Current().name == name);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!has_children) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (is_playing) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (selected) flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::PushID(idx);
    if (is_playing) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50F, 0.92F, 0.65F, 1.0F));
    }
    const bool open =
        ImGui::TreeNodeEx("##layer", flags, "%s%s", name.c_str(), is_playing ? "  " ICON_PLAY : "");
    if (is_playing) ImGui::PopStyleColor();

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
        Scene::Select({.kind = Scene::Selection::Kind::Layer, .path = name, .name = name});
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        state.PostCommand(AfpCmd::Wrap(AfpCmd::SwitchAnimation{.name = name, .label = ""}));
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Click to select, double-click to play / replay.");
    }

    if (has_children && open) {
        int ci = 0;
        for (const auto& c : status.mc_tree.children)
            RenderSceneNode(state, active, status, cfg, c, overrides, lower_filter, ci++);
        RenderUnresolvedSlots(status, cfg);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void RenderAddSlotRow(App::IfsConfig& cfg) {
    static char buf[128] = {};
    ImGui::SetNextItemWidth(Gui::Dpi::S(-72.0F));
    ImGui::InputTextWithHint("##new_slot", "add slot by clip path, e.g. coin", buf, sizeof(buf));
    ImGui::SameLine();
    if (ImGui::Button("Add", ImVec2(-FLT_MIN, 0)) && buf[0] != 0) {
        bool exists = false;
        for (auto& s : cfg.slots) {
            if (s.path == buf) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            App::VariantSlot s;
            s.path = buf;
            s.default_bitmap = buf;
            s.visible = true;
            s.is_valid = false;
            cfg.slots.push_back(std::move(s));
        }
        buf[0] = 0;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Register a clip path as a variant slot. The render thread\n"
                          "probes it next frame; unresolved slots stay listed under the\n"
                          "playing layer until they resolve.");
    }
}

}

void RenderScenePane() {
    auto& state = App::Global();
    auto status = state.GetStatus();
    std::string const active = state.ActiveIfs();

    static std::string s_last_active;
    if (active != s_last_active) {
        s_last_active = active;
        Scene::Reset();
    }

    if (active.empty()) {
        Gui::SectionHeader(ICON_SCENE, "Scene", nullptr);
        ImGui::TextDisabled("Select an IFS on the left to inspect its clips.");
        return;
    }

    auto& cfg = state.MutConfig(active);
    char suffix[96];
    snprintf(suffix, sizeof(suffix), "%zu layer%s, %zu bitmaps, %zu slot%s", cfg.anim_names.size(),
             cfg.anim_names.size() == 1 ? "" : "s", cfg.bitmap_names.size(), cfg.slots.size(),
             cfg.slots.size() == 1 ? "" : "s");
    Gui::SectionHeader(ICON_SCENE, "Scene", suffix);

    if (cfg.anim_names.empty()) {
        ImGui::TextDisabled("No layers listed in afplist.xml.");
        return;
    }

    static char filter_buf[128] = {};
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##scene_filter", "find clip...", filter_buf, sizeof(filter_buf));
    std::string const lower_filter = Lower(filter_buf);
    ImGui::Spacing();

    const auto overrides = state.GetSublayerOverrides(active);

    ImGui::BeginChild("scene_scroll", ImVec2(0.0F, Gui::Dpi::S(-38.0F)), 0);
    int idx = 0;
    for (const auto& name : cfg.anim_names)
        RenderLayerRow(state, active, status, cfg, overrides, lower_filter, name, idx++);
    ImGui::EndChild();

    ImGui::Spacing();
    RenderAddSlotRow(cfg);
}

}
