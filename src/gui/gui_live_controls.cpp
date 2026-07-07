#include "gui_live_controls.h"
#include "../state/app_state.h"
#include "imgui.h"
#include "state/telemetry.h"

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace Panels::LiveControls {

namespace {

void PostSeekPaused(App::State& state, int frame, int maxf) {
    frame = std::max(frame, 0);
    frame = std::min(frame, maxf);
    App::Request r;
    r.seek_frame = true;
    r.seek_to_frame = frame;
    state.PostRequest(std::move(r));
    auto o = state.GetLiveOverrides();
    o.paused = true;
    state.SetLiveOverrides(o);
}

void DrawPausedCheckbox(App::State& state, bool paused) {
    if (ImGui::Checkbox("Paused", &paused)) {
        App::Request r;
        r.set_paused = true;
        r.paused_value = paused;
        state.PostRequest(std::move(r));
        auto o = state.GetLiveOverrides();
        o.paused = paused;
        state.SetLiveOverrides(o);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Pause / resume playback by setting the stream playback speed to\n"
                          "0 / 1 (afp_stream_set_speed). Matches the debug viewer's\n"
                          "RETURN+SHIFT toggle. Forced to running while exporting.");
    }
}

}

void RenderSeekControls() {
    auto& state = App::Global();
    auto live = state.GetLiveState();
    auto ov = state.GetLiveOverrides();

    const bool exporting = state.GetExport().phase == App::ExportPhase::Capturing;

    ImGui::Spacing();
    ImGui::TextDisabled("Playback (debug viewer TIME / PLAY-PAUSE)");

    if (exporting) ImGui::BeginDisabled();

    const int maxf = live.mc_total > 0 ? (int)live.mc_total - 1 : 0;
    int cur = (int)live.mc_cur;
    cur = std::min(cur, maxf);

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderInt("##seek_frame", &cur, 0, maxf, "frame %d")) {
        PostSeekPaused(state, cur, maxf);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Seek the master movie clip to an absolute frame and play from\n"
                          "there (afp_mc_control 0xF08). Pauses playback on seek, mirroring\n"
                          "the AFP debug viewer's LEFT/RIGHT TIME controls.");
    }

    auto step = [&](int d) {
        int const total = live.mc_total > 0 ? (int)live.mc_total : 1;
        PostSeekPaused(state, ((cur + d) % total + total) % total, maxf);
    };
    if (ImGui::SmallButton("-100")) step(-100);
    ImGui::SameLine();
    if (ImGui::SmallButton("-1")) step(-1);
    ImGui::SameLine();
    if (ImGui::SmallButton("+1")) step(+1);
    ImGui::SameLine();
    if (ImGui::SmallButton("+100")) step(+100);

    DrawPausedCheckbox(state, ov.paused);

    if (exporting) {
        ImGui::EndDisabled();
        ImGui::TextDisabled("(seek / pause disabled during export)");
    }
}

namespace {

void DrawLoopTrimControls(App::State::LiveOverrides& ov, bool& changed) {
    ImGui::Text("Continuous loop");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputInt("##live_cont", &ov.continuous_loop_mode, 0, 0)) {
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Continuous-loop mode applied to the active stream.\n"
                          "-1 = explicit OFF (clear the CLayer flag - master\n"
                          "     reverts to gotoAndStop saturation).\n"
                          " 0 = leave unchanged (engine default).\n"
                          " 1 = explicit ON (apply the BG dispatcher's continuous-\n"
                          "     loop flag sequence - master keeps advancing past\n"
                          "     total_length, sub-clips evolve naturally; required\n"
                          "     for BG 20).\n\n"
                          "Toggling this re-applies on the NEXT stream switch - load\n"
                          "or hot-swap the animation to take effect.");
    }

    ImGui::Spacing();
    ImGui::Text("Trim frames");
    ImGui::SetNextItemWidth(-FLT_MIN);
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

void DrawBackgroundCycler(App::State::LiveOverrides& ov, bool& changed) {
    static const char* kBgNames[6] = {"default", "grey", "black", "red", "green", "blue"};
    int const slot = (ov.bg_color_index < 0 || ov.bg_color_index > 4) ? 0 : ov.bg_color_index + 1;
    char btn[48];
    snprintf(btn, sizeof(btn), "Background: %s", kBgNames[slot]);
    if (ImGui::Button(btn)) {
        ov.bg_color_index = ((ov.bg_color_index + 2) % 6) - 1;
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Preview background (AFP debug viewer F4).\n"
                          "Cycles default (transparent) -> grey -> black ->\n"
                          "red -> green -> blue. Affects the live preview only;\n"
                          "the export uses its own Background setting.");
    }
}

void DrawFilterMcNameControls(App::State::LiveOverrides& ov, bool& changed) {
    ImGui::Spacing();
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
                          "children, ord 0x079) and list them below. Name-type 0 also draws\n"
                          "each over the preview at its clip position; type 1 lists them in\n"
                          "a fixed column (matches the debug viewer's F3 / F6).");
    }
    if (ov.show_mc_names) {
        ImGui::Indent(18.0F);
        if (ImGui::RadioButton("At clip pos", ov.mc_name_type == 0)) {
            ov.mc_name_type = 0;
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Fixed column", ov.mc_name_type == 1)) {
            ov.mc_name_type = 1;
            changed = true;
        }
        ImGui::Unindent(18.0F);
    }
}

void DrawActiveLabelLine(const App::Status& status) {
    const auto& labels = status.labels;
    if (status.active_label.empty() || labels.empty()) return;
    int idx = -1;
    for (size_t i = 0; i < labels.size(); ++i) {
        if (labels[i].name == status.active_label) {
            idx = (int)i;
            break;
        }
    }
    if (idx >= 0) {
        ImGui::Text("label:  %s (%d/%zu)", status.active_label.c_str(), idx + 1, labels.size());
    } else {
        ImGui::Text("label:  %s", status.active_label.c_str());
    }
}

void DrawMcPlayheadLines(const App::Status& status, const App::State::LiveState& live) {
    ImGui::Text("cur:    %u / %u%s", live.mc_cur, live.mc_total,
                live.label_active ? " (label)" : "");
    ImGui::Text("loops:  %u", live.mc_wrap_count);
    if ((live.mc_w != 0U) || (live.mc_h != 0U))
        ImGui::Text("size:   %u x %u", live.mc_w, live.mc_h);
    DrawActiveLabelLine(status);
}

void DrawLiveStateSection(const App::Status& status, const App::State::LiveState& live) {
    ImGui::TextDisabled("Live state");
    if (live.have_layer_info || live.have_mc_playhead) {
        if (live.have_mc_playhead) DrawMcPlayheadLines(status, live);
        if (live.have_layer_info) ImGui::Text("raw cur:%u / %u", live.cur_pos, live.total_length);
        ImGui::Text("frame:  %d (since last switch)", live.frames_since_switch);
        ImGui::Text("flags0: 0x%08x", live.flags0);
        ImGui::Text("stream: 0x%08x", live.stream_id);
        ImGui::Text("master_complete: %s", live.master_complete ? "YES" : "no");
        ImGui::Text("filter: %s", live.filter_on ? "ON" : "off");
    } else {
        ImGui::TextDisabled("(no active layer info)");
    }
}

void DrawFileInfoSection(const App::State::LiveState& live) {
    if (live.have_file_info) {
        ImGui::Spacing();
        ImGui::TextDisabled("File info");
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
        ImGui::TextDisabled("locale:    n/a (game region)");
    }
}

void DrawMcNamesList(const App::Status& status) {
    if (!status.mc_children.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("MC names (%zu)", status.mc_children.size());
        ImGui::BeginChild("mc_names_list", ImVec2(0, 140.0F), 1);
        for (const auto& c : status.mc_children) {
            if (c.have_pos) {
                ImGui::Text("%s  (%.0f, %.0f)", c.name.c_str(), c.x, c.y);
            } else {
                ImGui::Text("%s", c.name.c_str());
            }
        }
        ImGui::EndChild();
    }
}

}

void RenderOverridePanel() {
    auto& state = App::Global();
    auto status = state.GetStatus();
    const bool ddr = state.IsDdrMode();

    ImGui::TextDisabled("For: %s", status.playing_animation.empty()
                                       ? "(no animation playing)"
                                       : status.playing_animation.c_str());

    auto ov = state.GetLiveOverrides();
    bool changed = false;

    if (!ddr) DrawLoopTrimControls(ov, changed);

    ImGui::Spacing();
    DrawBackgroundCycler(ov, changed);

    if (!ddr) DrawFilterMcNameControls(ov, changed);

    if (changed) {
        state.SetLiveOverrides(ov);
    }

    if (ImGui::Button("Reset live overrides")) {
        state.SetLiveOverrides(App::State::LiveOverrides{});
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    auto live = state.GetLiveState();
    DrawLiveStateSection(status, live);
    if (live.have_file_info) DrawFileInfoSection(live);
    if (ov.show_mc_names) DrawMcNamesList(status);
}

void RenderLabelsPanel() {
    auto& state = App::Global();
    const auto& labels = state.GetStatus().labels;
    if (labels.empty()) {
        ImGui::TextDisabled("This animation has no frame labels.");
        return;
    }
    for (size_t i = 0; i < labels.size(); ++i) {
        char row[320];
        snprintf(row, sizeof(row), "%s   (frame %d)##lbl%zu",
                 labels[i].name.empty() ? "(unnamed)" : labels[i].name.c_str(), labels[i].frame, i);
        if (ImGui::Selectable(row)) {
            App::Request r;
            r.goto_label = true;
            r.goto_label_name = labels[i].name;
            state.PostRequest(std::move(r));
        }
    }
}

namespace {
void RenderSubLayerNode(App::State& state, const std::string& active, const App::SubLayerNode& node,
                        const std::vector<std::pair<std::string, bool>>& overrides, int idx) {
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
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (is_leaf) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    const bool open = ImGui::TreeNodeEx(node.name.c_str(), flags);
    if (!is_leaf) {
        if (ImGui::IsItemToggledOpen()) state.SetSublayerExpanded(node.path, open);
        if (open) {
            int ci = 0;
            for (const auto& c : node.children)
                RenderSubLayerNode(state, active, c, overrides, ci++);
            ImGui::TreePop();
        }
    }
    ImGui::PopID();
}
}

void RenderSubLayersPanel() {
    auto& state = App::Global();
    if (state.IsDdrMode()) {
        ImGui::TextDisabled("Sub-layer toggles are unavailable for DDR (no enumerate).");
        return;
    }
    const std::string active = state.ActiveIfs();
    if (active.empty()) {
        ImGui::TextDisabled("(no IFS loaded)");
        return;
    }
    const auto status = state.GetStatus();
    if (status.mc_tree.children.empty()) {
        ImGui::TextDisabled("This layer has no named child sub-clips.");
        return;
    }
    const auto overrides = state.GetSublayerOverrides(active);
    int i = 0;
    for (const auto& c : status.mc_tree.children)
        RenderSubLayerNode(state, active, c, overrides, i++);
}

}
