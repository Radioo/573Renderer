#include "gui_panels_internal.h"
#include "gui_style.h"
#include "gui_widgets.h"
#include "panel_registry.h"
#include "../backend/afp_commands.h"
#include "../state/app_state.h"
#include "imgui.h"
#include "state/ifs_catalog.h"
#include "state/telemetry.h"

#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace Panels {

namespace {

const App::SubLayerNode* FindNodeByPath(const App::SubLayerNode& node, const std::string& path) {
    if (node.path == path) return &node;
    for (const auto& c : node.children) {
        const App::SubLayerNode* hit = FindNodeByPath(c, path);
        if (hit != nullptr) return hit;
    }
    return nullptr;
}

App::VariantSlot* FindSlotFor(App::IfsConfig& cfg, const Scene::Selection& sel) {
    for (auto& s : cfg.slots) {
        if (s.path == sel.path || s.path == sel.name) return &s;
    }
    return nullptr;
}

void DrawSlotBitmapCombo(App::IfsConfig& cfg, App::VariantSlot& slot) {
    const bool is_default = (!slot.bitmap.empty() && slot.bitmap == slot.default_bitmap);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const char* preview = slot.bitmap.empty() || is_default ? "(default)" : slot.bitmap.c_str();
    if (!ImGui::BeginCombo("##bitmap", preview)) return;
    if (ImGui::Selectable("(default)", slot.bitmap.empty() && !slot.bitmap_override)) {
        slot.bitmap.clear();
        slot.bitmap_override = false;
        App::Global().PostCommand(AfpCmd::Wrap(AfpCmd::ForceReplay{}));
    }
    for (auto& b : cfg.bitmap_names) {
        bool const selected = (!is_default && slot.bitmap == b);
        if (ImGui::Selectable(b.c_str(), selected)) {
            slot.bitmap = b;
            slot.bitmap_override = true;
        }
        if (selected) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
}

void DrawSlotBitmapInput(App::VariantSlot& slot) {
    char buf[128] = {};
    size_t n = slot.bitmap.size();
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, slot.bitmap.data(), n);
    buf[n] = 0;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##bitmap", "bitmap name (blank = IFS default)", buf,
                                 sizeof(buf))) {
        slot.bitmap = buf;
        slot.bitmap_override = (buf[0] != '\0');
    }
}

void DrawSlotProperties(App::IfsConfig& cfg, App::VariantSlot& slot) {
    ImGui::Spacing();
    ImGui::TextDisabled("Variant slot");
    if (!slot.is_valid) {
        ImGui::SameLine();
        ImGui::TextDisabled("(unresolved)");
    }
    ImGui::Checkbox("Slot visible", &slot.visible);
    if (!cfg.bitmap_names.empty()) {
        DrawSlotBitmapCombo(cfg, slot);
    } else {
        DrawSlotBitmapInput(slot);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Swap the clip's bitmap. \"(default)\" drops the override and\n"
                          "replays the master so the timeline re-authors the original\n"
                          "bitmap (afp has no restore-authored call).");
    }
}

void DrawLayerProperties(App::State& state, const Scene::Selection& sel,
                         const App::Status& status) {
    ImGui::TextDisabled("afplist layer");
    const bool is_playing = (sel.name == status.playing_animation);
    if (is_playing) {
        auto live = state.GetLiveState();
        if (live.have_mc_playhead) {
            Gui::PushMonoFont();
            ImGui::Text("%u frames", live.mc_total);
            ImGui::PopFont();
        }
        ImGui::TextColored(ImVec4(0.50F, 0.92F, 0.65F, 1.0F), "playing");
    }
    ImGui::Spacing();
    if (ImGui::Button(is_playing ? "Replay" : "Play", ImVec2(110, 0))) {
        state.PostCommand(AfpCmd::Wrap(AfpCmd::SwitchAnimation{.name = sel.name, .label = ""}));
    }
}

void DrawChildProperties(const Scene::Selection& sel, const App::Status& status,
                         App::IfsConfig& cfg) {
    Gui::PushMonoFont();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", sel.path.c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();

    for (const auto& c : status.mc_children) {
        if (c.name == sel.name && c.have_pos) {
            Gui::PushMonoFont();
            ImGui::Text("position  %.0f, %.0f", c.x, c.y);
            ImGui::PopFont();
            break;
        }
    }
    if (FindNodeByPath(status.mc_tree, sel.path) == nullptr) {
        ImGui::TextDisabled("(not in the enumerated child tree)");
    }

    App::VariantSlot* slot = FindSlotFor(cfg, sel);
    if (slot != nullptr) DrawSlotProperties(cfg, *slot);
}

}

void RenderPropertiesTab() {
    auto& state = App::Global();
    const Scene::Selection& sel = Scene::Current();
    std::string const active = state.ActiveIfs();

    if (sel.kind == Scene::Selection::Kind::None || active.empty()) {
        ImGui::TextDisabled("Select a layer or clip in the Scene pane.");
        return;
    }

    auto status = state.GetStatus();
    auto& cfg = state.MutConfig(active);

    Gui::PushHeaderFont();
    ImGui::TextUnformatted(sel.name.c_str());
    ImGui::PopFont();
    ImGui::Spacing();

    if (sel.kind == Scene::Selection::Kind::Layer) {
        DrawLayerProperties(state, sel, status);
    } else {
        DrawChildProperties(sel, status, cfg);
    }
}

namespace {

void DrawLoopMasterRow() {
    bool loop = App::Global().GetLoopMaster();
    if (ImGui::Checkbox("Loop master animation", &loop)) {
        App::Global().SetLoopMaster(loop);
        App::SaveCurrentSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("When the master animation reaches the end of\n"
                          "its timeline, re-play it from frame 0. Useful\n"
                          "for short title clips that otherwise freeze\n"
                          "on their last authored frame.");
    }
}

void DrawRootLoopRow() {
    auto mode = App::Global().GetRootLoopMode();
    int idx = (mode == App::State::RootLoopMode::Force) ? 1 : 0;
    const char* kItems[2] = {"Auto-hold", "Force loop"};
    ImGui::TextDisabled("Loop root");
    if (Gui::Segmented("##root_loop", kItems, 2, &idx)) {
        App::Global().SetRootLoopMode(idx == 1 ? App::State::RootLoopMode::Force
                                               : App::State::RootLoopMode::Hold);
        App::SaveCurrentSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("How a scene-BG root that reaches its end is driven.\n"
                          "Auto-hold (game default): mount once, let the root\n"
                          "  play once and HOLD while nested children keep running.\n"
                          "Force loop: re-drive the root to frame 0 each cycle\n"
                          "  (ForceReplay + the continuous-loop flag sequence). Needed\n"
                          "  for one-shot masters (bg_common).\n\n"
                          "Applies to the live preview now and to the next non-label\n"
                          "export. Persisted across restarts.");
    }
}

void DrawContinuousLoopRow(App::State::LiveOverrides& ov, bool& changed) {
    int idx = ov.continuous_loop_mode + 1;
    const char* kItems[3] = {"OFF", "default", "ON"};
    ImGui::TextDisabled("Continuous loop");
    if (Gui::Segmented("##live_cont", kItems, 3, &idx)) {
        ov.continuous_loop_mode = idx - 1;
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Continuous-loop mode applied to the active stream.\n"
                          "OFF: clear the CLayer flag - master reverts to\n"
                          "     gotoAndStop saturation.\n"
                          "default: leave unchanged (engine default).\n"
                          "ON:  apply the BG dispatcher's continuous-loop flag\n"
                          "     sequence - master keeps advancing past total_length,\n"
                          "     sub-clips evolve naturally; required for BG 20.\n\n"
                          "Re-applied on the NEXT stream switch - load or hot-swap\n"
                          "the animation to take effect.");
    }
}

void DrawTrimRow(App::State::LiveOverrides& ov, bool& changed) {
    ImGui::TextDisabled("Trim frames");
    ImGui::SetNextItemWidth(120.0F);
    if (ImGui::InputInt("##live_trim", &ov.trim_frames, 0, 0)) {
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Live-preview trim point.\n"
                          "0   = no trim (loop forever via engine).\n"
                          ">0  = at frame N (rendered frames since the last\n"
                          "      stream-switch / hot-swap), restart the master\n"
                          "      via ForceReplay. Lets you watch a loop of\n"
                          "      exactly N frames at full framerate to inspect\n"
                          "      the smoothness of the wrap.");
    }
}

void DrawMasterScaleRow() {
    float scale = App::Global().GetMasterScale();
    ImGui::TextDisabled("Master scale");
    ImGui::SetNextItemWidth(-124.0F);
    if (ImGui::SliderFloat("##master_scale", &scale, 0.25F, 4.0F, "%.2fx")) {
        App::Global().SetMasterScale(scale);
        App::SaveCurrentSettings();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("1.0x##scale_reset")) {
        App::Global().SetMasterScale(1.0F);
        App::SaveCurrentSettings();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("1.5x##scale_sdvx_old")) {
        App::Global().SetMasterScale(1.5F);
        App::SaveCurrentSettings();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Multiplies the master stream's transform matrix by the\n"
                          "given factor. Default 1.0x.\n\n"
                          "1.5x preview matches what SDVX 7's BG dispatcher applies to\n"
                          "SDVX-I-through-IV-era 720x1280 select_bg variants on a\n"
                          "1080x1920 game.");
    }
}

void DrawBackgroundRow(App::State::LiveOverrides& ov, bool& changed) {
    int idx = (ov.bg_color_index < -1 || ov.bg_color_index > 4) ? 0 : ov.bg_color_index + 1;
    const char* kItems[6] = {"default", "grey", "black", "red", "green", "blue"};
    ImGui::TextDisabled("Background");
    if (Gui::Segmented("##bg_color", kItems, 6, &idx)) {
        ov.bg_color_index = idx - 1;
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Preview background (AFP debug viewer F4).\n"
                          "default = transparent. Affects the live preview only;\n"
                          "the export uses its own Background setting.");
    }
}

void DrawFilterMcNameRows(App::State::LiveOverrides& ov, bool& changed) {
    if (ImGui::Checkbox("Filter (F7)", &ov.filter_enabled)) {
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Toggle the AFP layer filter (debug viewer F7). Calls afp-core\n"
                          "set-filter (ord 0x032) on the active stream with the filter id\n"
                          "0x80000000|enable - the same call the scene's CLayer slot-32\n"
                          "wrapper makes.");
    }

    if (ImGui::Checkbox("Show MC names (F3)", &ov.show_mc_names)) {
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enumerate the master's named child clips (afp_mc_enumerate_\n"
                          "children, ord 0x079). Name-type \"at clip pos\" draws each name\n"
                          "over the preview at its clip position; \"column\" lists them in\n"
                          "the Live tab (matches the debug viewer's F3 / F6). Also feeds\n"
                          "the child positions shown in the Scene tree.");
    }
    if (ov.show_mc_names) {
        ImGui::Indent(24.0F);
        const char* kItems[2] = {"at clip pos", "column"};
        if (Gui::Segmented("##mc_name_type", kItems, 2, &ov.mc_name_type)) changed = true;
        ImGui::Unindent(24.0F);
    }
}

void DrawResetOverridesRow(App::State& state) {
    ImGui::Spacing();
    if (ImGui::Button("Reset live overrides")) {
        state.SetLiveOverrides(App::State::LiveOverrides{});
    }
}

}

void RenderRenderTabModern() {
    auto& state = App::Global();
    const auto before = state.GetLiveOverrides();
    auto ov = before;
    bool changed = false;

    DrawLoopMasterRow();
    ImGui::Spacing();
    DrawRootLoopRow();
    ImGui::Spacing();
    DrawContinuousLoopRow(ov, changed);
    ImGui::Spacing();
    DrawTrimRow(ov, changed);
    ImGui::Spacing();
    DrawMasterScaleRow();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    DrawBackgroundRow(ov, changed);
    ImGui::Spacing();
    DrawFilterMcNameRows(ov, changed);

    if (changed) state.ApplyLiveOverridesDelta(before, ov);
    ImGui::Spacing();
    DrawResetOverridesRow(state);
}

void RenderRenderTabDdr() {
    auto& state = App::Global();
    const auto before = state.GetLiveOverrides();
    auto ov = before;
    bool changed = false;

    DrawBackgroundRow(ov, changed);

    if (changed) state.ApplyLiveOverridesDelta(before, ov);
    DrawResetOverridesRow(state);
}

namespace {

void DrawLiveStateSection(const App::Status& status, const App::State::LiveState& live) {
    ImGui::TextDisabled("Live state");
    Gui::PushMonoFont();
    if (live.have_layer_info || live.have_mc_playhead) {
        if (live.have_mc_playhead) {
            ImGui::Text("cur:    %u / %u%s", live.mc_cur, live.mc_total,
                        live.label_active ? " (label)" : "");
            ImGui::Text("loops:  %u", live.mc_wrap_count);
            if ((live.mc_w != 0U) || (live.mc_h != 0U))
                ImGui::Text("size:   %u x %u", live.mc_w, live.mc_h);
            if (!status.active_label.empty()) {
                ImGui::Text("label:  %s", status.active_label.c_str());
            }
        }
        if (live.have_layer_info) ImGui::Text("raw:    %u / %u", live.cur_pos, live.total_length);
        ImGui::Text("frame:  %d (since last switch)", live.frames_since_switch);
        ImGui::Text("flags0: 0x%08x", live.flags0);
        ImGui::Text("stream: 0x%08x", live.stream_id);
        ImGui::Text("master_complete: %s", live.master_complete ? "YES" : "no");
        ImGui::Text("filter: %s", live.filter_on ? "ON" : "off");
    } else {
        ImGui::TextDisabled("(no active layer info)");
    }
    ImGui::PopFont();
}

void DrawFileInfoSection(const App::State::LiveState& live) {
    ImGui::Spacing();
    ImGui::TextDisabled("File info");
    Gui::PushMonoFont();
    auto ver = [](uint32_t v, char* out, size_t n) {
        snprintf(out, n, "%u.%u.%u", (v >> 16) & 0xFFFF, (v >> 8) & 0xFF, v & 0xFF);
    };
    char cbuf[24];
    char pbuf[24];
    char abuf[24];
    ver(live.conv_ver, cbuf, sizeof(cbuf));
    ver(live.pkg_ver, pbuf, sizeof(pbuf));
    ver(live.afp_ver, abuf, sizeof(abuf));
    ImGui::Text("converter: %s (%s)", cbuf,
                (live.conv_engine[0] != 0) ? live.conv_engine.data() : "???");
    ImGui::Text("package:   %s", pbuf);
    ImGui::Text("afp:       %s", abuf);
    if (live.size_bytes != 0U)
        ImGui::Text("size:      %llu bytes", (unsigned long long)live.size_bytes);
    if (live.load_time_ms != 0U)
        ImGui::Text("loaded:    %llu (epoch ms)", (unsigned long long)live.load_time_ms);
    ImGui::PopFont();
}

void DrawMcNamesList(const App::Status& status) {
    if (status.mc_children.empty()) return;
    ImGui::Spacing();
    ImGui::TextDisabled("MC names (%zu)", status.mc_children.size());
    Gui::PushMonoFont();
    ImGui::BeginChild("mc_names_list", ImVec2(0, 140.0F), 1);
    for (const auto& c : status.mc_children) {
        if (c.have_pos) {
            ImGui::Text("%s  (%.0f, %.0f)", c.name.c_str(), c.x, c.y);
        } else {
            ImGui::Text("%s", c.name.c_str());
        }
    }
    ImGui::EndChild();
    ImGui::PopFont();
}

}

void RenderLiveTab() {
    auto& state = App::Global();
    auto status = state.GetStatus();
    auto live = state.GetLiveState();
    auto ov = state.GetLiveOverrides();

    DrawLiveStateSection(status, live);
    if (live.have_file_info) DrawFileInfoSection(live);
    if (ov.show_mc_names && ov.mc_name_type == 1) DrawMcNamesList(status);
}

void RenderInspectorPane() {
    static std::vector<const Gui::PanelDesc*> tabs;
    Gui::CollectActivePanels(Gui::PanelSlot::InspectorTab, tabs);
    if (tabs.empty()) return;
    if (ImGui::BeginTabBar("##inspector_tabs", ImGuiTabBarFlags_FittingPolicyResizeDown)) {
        for (const auto* tab : tabs) {
            if (ImGui::BeginTabItem(tab->tab_label)) {
                ImGui::Spacing();
                tab->draw();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}

}
