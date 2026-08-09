#include "gui_export_panel.h"
#include "gui_icons.h"
#include "gui_layout_constants.h"
#include "gui_panels_internal.h"
#include "gui_style.h"
#include "../backend/afp_commands.h"
#include "../state/app_state.h"
#include "imgui.h"
#include "state/telemetry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace Panels {

namespace {

void PostSeekPaused(App::State& state, int frame, int maxf) {
    frame = std::max(frame, 0);
    frame = std::min(frame, maxf);
    state.PostCommand(AfpCmd::Wrap(AfpCmd::SeekFrame{.frame = frame}));
    state.MutateLiveOverrides([](App::State::LiveOverrides& o) { o.paused = true; });
}

void PostTogglePause(App::State& state) {
    bool new_paused = false;
    state.MutateLiveOverrides([&new_paused](App::State::LiveOverrides& o) {
        o.paused = !o.paused;
        new_paused = o.paused;
    });
    state.PostCommand(AfpCmd::Wrap(AfpCmd::SetPaused{.paused = new_paused}));
}

void StepWrapped(App::State& state, const App::State::LiveState& live, int delta) {
    int const total = live.mc_total > 0 ? (int)live.mc_total : 1;
    int const maxf = total - 1;
    int const cur = std::min((int)live.mc_cur, maxf);
    PostSeekPaused(state, (((cur + delta) % total) + total) % total, maxf);
}

bool TransportButton(const char* icon, const char* tip) {
    bool const pressed = ImGui::Button(icon, ImVec2(34, 0));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    return pressed;
}

void DrawLabelCombo(App::State& state, const App::Status& status) {
    ImGui::SameLine(0.0F, 12.0F);
    ImGui::SetNextItemWidth(170.0F);
    const char* preview =
        status.active_label.empty() ? "go to label..." : status.active_label.c_str();
    if (ImGui::BeginCombo("##tl_labels", preview)) {
        for (size_t i = 0; i < status.labels.size(); i++) {
            const auto& l = status.labels[i];
            char row[192];
            snprintf(row, sizeof(row), "%s   (frame %d)##lbl%zu",
                     l.name.empty() ? "(unnamed)" : l.name.c_str(), l.frame, i);
            const bool selected = (!status.active_label.empty() && l.name == status.active_label);
            if (ImGui::Selectable(row, selected)) {
                state.PostCommand(AfpCmd::Wrap(AfpCmd::GotoLabel{.name = l.name}));
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Jump to a frame label and play from there (label playback).\n"
                          "Labels are also the ticks on the track; hover a tick for its\n"
                          "name, click it to jump.");
    }
}

void DrawTransportRow(App::State& state, const App::Status& status,
                      const App::State::LiveState& live, bool exporting) {
    auto ov = state.GetLiveOverrides();

    if (TransportButton(ICON_JUMP_BACK, "Step back 100 frames (Shift+Left).\n"
                                        "Wraps around the master timeline; seeking pauses.")) {
        StepWrapped(state, live, -100);
    }
    ImGui::SameLine(0.0F, 3.0F);
    if (TransportButton(ICON_STEP_BACK, "Step back 1 frame (Left).")) StepWrapped(state, live, -1);
    ImGui::SameLine(0.0F, 3.0F);
    if (TransportButton(ov.paused ? ICON_PLAY : ICON_PAUSE,
                        "Play / pause (Space). Sets the stream playback speed to\n"
                        "1 / 0 (afp_stream_set_speed), matching the debug viewer's\n"
                        "RETURN+SHIFT toggle. Forced to running while exporting.")) {
        PostTogglePause(state);
    }
    ImGui::SameLine(0.0F, 3.0F);
    if (TransportButton(ICON_STEP_FWD, "Step forward 1 frame (Right).")) {
        StepWrapped(state, live, +1);
    }
    ImGui::SameLine(0.0F, 3.0F);
    if (TransportButton(ICON_JUMP_FWD, "Step forward 100 frames (Shift+Right).")) {
        StepWrapped(state, live, +100);
    }

    ImGui::SameLine(0.0F, 14.0F);
    Gui::PushMonoFont();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%u / %u", live.mc_cur, live.mc_total);
    ImGui::SameLine(0.0F, 12.0F);
    ImGui::TextDisabled("loop %u", live.mc_wrap_count);
    ImGui::PopFont();

    if (!status.labels.empty()) DrawLabelCombo(state, status);

    if (exporting) {
        ImGui::SameLine(0.0F, 14.0F);
        ImGui::TextDisabled("(seek / pause disabled during export)");
    }
}

int LabelHitTest(const App::Status& status, uint32_t total, float x0, float w, float mouse_x) {
    int hit = -1;
    for (size_t i = 0; i < status.labels.size(); ++i) {
        float const lx = x0 + (w * ((float)status.labels[i].frame / (float)total));
        if (std::fabs(mouse_x - lx) <= 5.0F) hit = (int)i;
    }
    return hit;
}

void DrawTrackMarkers(ImDrawList* dl, const App::Status& status, uint32_t total, float x0, float y0,
                      float w, float h) {
    const ImU32 tick_col = ImGui::GetColorU32(ImVec4(0.851F, 0.627F, 0.247F, 0.85F));
    for (const auto& l : status.labels) {
        float const lx = x0 + (w * ((float)l.frame / (float)total));
        dl->AddLine(ImVec2(lx, y0), ImVec2(lx, y0 + h), tick_col, 1.0F);
    }
}

void DrawTrack(App::State& state, const App::Status& status, const App::State::LiveState& live,
               const App::ExportState& ex, bool exporting) {
    const float w = ImGui::GetContentRegionAvail().x;
    const float h = 22.0F;
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton("##tl_track", ImVec2(w, h));
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    dl->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + h), ImGui::GetColorU32(ImGuiCol_ScrollbarBg));
    dl->AddRect(p0, ImVec2(p0.x + w, p0.y + h), ImGui::GetColorU32(ImGuiCol_Border));

    const uint32_t total = live.mc_total;
    if (total == 0) return;

    float const frac = std::clamp((float)live.mc_cur / (float)total, 0.0F, 1.0F);
    dl->AddRectFilled(p0, ImVec2(p0.x + (w * frac), p0.y + h), ImGui::GetColorU32(ImGuiCol_Header));

    if (exporting && ex.frames_captured > 0) {
        float const cap = std::clamp((float)ex.frames_captured / (float)total, 0.0F, 1.0F);
        dl->AddRectFilled(p0, ImVec2(p0.x + (w * cap), p0.y + h),
                          ImGui::GetColorU32(ImVec4(0.31F, 0.70F, 0.47F, 0.30F)));
    }

    DrawTrackMarkers(dl, status, total, p0.x, p0.y, w, h);

    float const head_x = p0.x + (w * frac);
    dl->AddRectFilled(ImVec2(head_x - 1.0F, p0.y - 2.0F), ImVec2(head_x + 1.0F, p0.y + h + 2.0F),
                      ImGui::GetColorU32(ImGuiCol_SliderGrabActive));

    if (exporting) return;

    const float mouse_x = ImGui::GetIO().MousePos.x;
    int const label_hit = hovered ? LabelHitTest(status, total, p0.x, w, mouse_x) : -1;
    if (label_hit >= 0) {
        const auto& l = status.labels[(size_t)label_hit];
        ImGui::SetTooltip("label %s (frame %d) - click to play from here",
                          l.name.empty() ? "(unnamed)" : l.name.c_str(), l.frame);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            state.PostCommand(AfpCmd::Wrap(AfpCmd::GotoLabel{.name = l.name}));
        }
        return;
    }
    if (hovered) {
        ImGui::SetTooltip("Drag to seek (afp_mc_control 0xF08). Seeking pauses playback,\n"
                          "mirroring the AFP debug viewer's TIME controls.");
    }
    if (active) {
        int const maxf = (int)total - 1;
        auto frame = (int)std::lround(((mouse_x - p0.x) / w) * (float)maxf);
        PostSeekPaused(state, frame, maxf);
    }
}

void HandleShortcuts(App::State& state, const App::State::LiveState& live, bool exporting) {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;

    if (ImGui::IsKeyPressed(ImGuiKey_E, false) && io.KeyCtrl) {
        Export::RequestOpen();
        return;
    }
    if (exporting) return;

    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) PostTogglePause(state);
    int const step = io.KeyShift ? 100 : 1;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) StepWrapped(state, live, -step);
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) StepWrapped(state, live, +step);
}

}

void RenderTimelineDock() {
    auto& state = App::Global();
    auto status = state.GetStatus();
    auto live = state.GetLiveState();
    App::ExportState const ex = state.GetExport();
    const bool exporting = (ex.phase == App::ExportPhase::Capturing);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_PopupBg));
    ImGui::BeginChild("##timeline_dock", ImVec2(0, Gui::kTimelineH), 1,
                      ImGuiWindowFlags_NoScrollbar);

    if (!status.scene_loaded) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Load an IFS to control playback.");
    } else {
        if (exporting) ImGui::BeginDisabled();
        DrawTransportRow(state, status, live, exporting);
        DrawTrack(state, status, live, ex, exporting);
        if (exporting) ImGui::EndDisabled();
        HandleShortcuts(state, live, exporting);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

}
