#include "gui_tl_internal.h"

#include "editor/preset_editor_state.h"
#include "editor/timeline_edits.h"
#include "editor/timeline_lanes.h"
#include "gui/gui_dpi.h"
#include "imgui.h"
#include "preset/doc/preset_document.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

constexpr float kChipWDips = 4.0F;
constexpr float kToggleGapDips = 2.0F;
constexpr float kNameMinWDips = 24.0F;
constexpr float kNameGapDips = 6.0F;

float ChipW() {
    return Gui::Dpi::S(kChipWDips);
}

float ToggleGap() {
    return Gui::Dpi::S(kToggleGapDips);
}

float NameMinW() {
    return Gui::Dpi::S(kNameMinWDips);
}

float NameGap() {
    return Gui::Dpi::S(kNameGapDips);
}

char g_rename[96] = {};
std::string g_rename_track;

struct Band {
    ImVec2 anchor = ImVec2(0.0F, 0.0F);
    bool active = false;
};

Band g_band;

float TogglesWidth() {
    return (3.0F * ToggleSide()) + (2.0F * ToggleGap());
}

void Toggle(Ctx& ctx, const char* prefix, const Doc::Track& track, bool on, const char* tip,
            bool (*edit)(Doc::Document&, std::string_view, bool)) {
    const std::string id = std::string(prefix) + track.id;
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImGui::GetStyleColorVec4(on ? ImGuiCol_CheckMark : ImGuiCol_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImGui::GetStyleColorVec4(on ? ImGuiCol_WindowBg : ImGuiCol_TextDisabled));
    const float side = ToggleSide();
    const bool pressed = ImGui::Button(id.c_str(), ImVec2(side, side));
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    if (!pressed) return;
    const std::string track_id = track.id;
    const bool wanted = !on;
    ApplyEdit(ctx, [edit, track_id, wanted](Doc::Document& document) {
        return edit(document, track_id, wanted);
    });
}

void HeaderMenu(Ctx& ctx, const Doc::Track& track) {
    const std::string track_id = track.id;
    if (ImGui::MenuItem("Rename track...")) {
        g_rename_track = track_id;
        snprintf(g_rename, sizeof(g_rename), "%s", track.name.c_str());
    }
    if (ImGui::MenuItem("Move up")) {
        ApplyEdit(ctx, [track_id](Doc::Document& document) {
            return Editor::MoveTrack(document, track_id, -1);
        });
    }
    if (ImGui::MenuItem("Move down")) {
        ApplyEdit(ctx, [track_id](Doc::Document& document) {
            return Editor::MoveTrack(document, track_id, 1);
        });
    }
    if (ImGui::MenuItem("Duplicate track")) {
        ApplyEdit(ctx, [track_id](Doc::Document& document) {
            return Editor::DuplicateTrack(document, track_id);
        });
    }
    if (ImGui::MenuItem("Change target...")) {
        ctx.editor->PostRequest(
            Editor::Request{.kind = Editor::RequestKind::AddTrack, .track_id = track_id});
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Delete track")) {
        ApplyEdit(ctx, [track_id](Doc::Document& document) {
            return Editor::DeleteTrack(document, track_id);
        });
    }
}

void RenameField(Ctx& ctx, const Doc::Track& track, float x, float y, float width) {
    if (g_rename_track != track.id) return;
    ImGui::SetCursorScreenPos(ImVec2(x, y + Gui::Dpi::S(2.0F)));
    ImGui::SetNextItemWidth(width);
    const std::string id = "###tl_rename_" + track.id;
    const bool done = ImGui::InputText(id.c_str(), g_rename, sizeof(g_rename),
                                       ImGuiInputTextFlags_EnterReturnsTrue);
    if (!done) return;
    const std::string track_id = track.id;
    const std::string name = g_rename;
    g_rename_track.clear();
    ApplyEdit(ctx, [track_id, name](Doc::Document& document) {
        return Editor::RenameTrack(document, track_id, name);
    });
}

void LaneMenu(Ctx& ctx, const Doc::Track& track, int frame) {
    const std::string track_id = track.id;
    if (ImGui::MenuItem("Add command here...")) {
        ctx.editor->PostRequest(Editor::Request{
            .kind = Editor::RequestKind::AddCommand, .track_id = track_id, .frame = frame});
    }
    if (ImGui::MenuItem("Paste at this frame", nullptr, false, !ctx.editor->Clipboard().empty())) {
        PasteAt(ctx, frame);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Add track above...")) {
        ctx.editor->PostRequest(Editor::Request{
            .kind = Editor::RequestKind::AddTrack, .track_id = track_id, .frame = -1});
    }
    if (ImGui::MenuItem("Add track below...")) {
        ctx.editor->PostRequest(Editor::Request{
            .kind = Editor::RequestKind::AddTrack, .track_id = track_id, .frame = 1});
    }
}

void DrawHeaderName(Ctx& ctx, const Doc::Track& track, float y, float height, float name_w) {
    const std::string head_id = "###tl_head_" + track.id;
    const float name_x = ctx.header_x + ChipW() + NameGap();
    ImGui::SetCursorScreenPos(ImVec2(name_x, y));
    ImGui::InvisibleButton(head_id.c_str(), ImVec2(name_w, height));

    const bool locked = track.locked;
    const ImU32 name_color = ImGui::GetColorU32(locked ? ImGuiCol_TextDisabled : ImGuiCol_Text);
    const float text_y = y + ((RowHeight() - ImGui::GetTextLineHeight()) * 0.5F);
    const std::string name = Ellipsized(track.name.empty() ? track.id : track.name, name_w);
    ctx.draw->AddText(ImVec2(name_x, text_y), name_color, name.c_str());

    if (ImGui::BeginPopupContextItem(("##tl_track_menu_" + track.id).c_str())) {
        HeaderMenu(ctx, track);
        ImGui::EndPopup();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s track '%s'%s%s\ndouble-click to rename, drag to reorder, "
                          "right-click for the track menu",
                          KindBadge(track.kind), track.id.c_str(),
                          track.target.empty() ? "" : (" -> " + track.target).c_str(),
                          locked ? " (locked)" : "");
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        g_rename_track = track.id;
        snprintf(g_rename, sizeof(g_rename), "%s", track.name.c_str());
    }
    if (!ImGui::IsItemActive() || !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) return;
    const float delta = ImGui::GetIO().MouseDelta.y;
    if (std::fabs(delta) <= height * 0.5F) return;
    const std::string track_id = track.id;
    const int step = delta > 0.0F ? 1 : -1;
    ApplyEdit(ctx, [track_id, step](Doc::Document& document) {
        return Editor::MoveTrack(document, track_id, step);
    });
}

void DrawHeaderToggles(Ctx& ctx, const Doc::Track& track, float x, float y) {
    const float side = ToggleSide();
    ImGui::SetCursorScreenPos(ImVec2(x, y + ((RowHeight() - side) * 0.5F)));
    Toggle(ctx, "M###tl_mute_", track, track.muted, "Mute: the evaluator skips this track.",
           &Editor::SetTrackMuted);
    ImGui::SameLine(0.0F, ToggleGap());
    Toggle(ctx, "S###tl_solo_", track, track.solo, "Solo: only solo tracks evaluate.",
           &Editor::SetTrackSolo);
    ImGui::SameLine(0.0F, ToggleGap());
    Toggle(ctx, "L###tl_lock_", track, track.locked, "Lock: this track rejects edits.",
           &Editor::SetTrackLocked);
}

void DrawHeaderBadge(const Ctx& ctx, const Doc::Track& track, float x, float y) {
    const float text_y = y + ((RowHeight() - ImGui::GetTextLineHeight()) * 0.5F);
    ctx.draw->AddText(ImVec2(x, text_y), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                      KindBadge(track.kind));
}

}

float ToggleSide() {
    const ImVec2 pad = ImGui::GetStyle().FramePadding;
    float glyph = 0.0F;
    for (const char* letter : {"M", "S", "L"})
        glyph = std::max(glyph, ImGui::CalcTextSize(letter).x);
    return std::max(ImGui::GetTextLineHeight() + (2.0F * pad.y), glyph + (2.0F * pad.x));
}

Editor::LaneMetrics LaneSizes() {
    return Editor::LaneMetricsFor(ImGui::GetTextLineHeight(), ImGui::GetStyle().FramePadding.y,
                                  Gui::Dpi::Scale());
}

float RowHeight() {
    return std::max(LaneSizes().row, ToggleSide() + Gui::Dpi::S(4.0F));
}

void DrawTrackHeader(Ctx& ctx, const Doc::Track& track, float y, float height) {
    ctx.draw->AddRectFilled(ImVec2(ctx.header_x, y), ImVec2(ctx.lane_x, y + height),
                            ImGui::GetColorU32(ImGuiCol_FrameBg));
    ctx.draw->AddRectFilled(ImVec2(ctx.header_x, y), ImVec2(ctx.header_x + ChipW(), y + height),
                            TrackKindColor(track.kind));
    ctx.draw->AddLine(ImVec2(ctx.header_x, y + height), ImVec2(ctx.lane_x, y + height),
                      ImGui::GetColorU32(ImGuiCol_Border), Gui::Dpi::S(1.0F));

    const float name_x = ctx.header_x + ChipW() + NameGap();
    const float toggles_x = ctx.lane_x - Gui::Dpi::S(4.0F) - TogglesWidth();
    const float badge_w = ImGui::CalcTextSize(KindBadge(track.kind)).x;
    const float badge_x = toggles_x - NameGap() - badge_w;
    const bool badge_fits = badge_x - NameGap() - name_x >= NameMinW();
    const float name_w =
        std::max(NameMinW(), (badge_fits ? badge_x : toggles_x) - NameGap() - name_x);

    DrawHeaderName(ctx, track, y, height, name_w);
    if (badge_fits) DrawHeaderBadge(ctx, track, badge_x, y);
    DrawHeaderToggles(ctx, track, toggles_x, y);
    RenameField(ctx, track, name_x, y, name_w);
}

void DrawTrackLane(Ctx& ctx, const Doc::Track& track, float y, float height) {
    const float right = ctx.lane_x + ctx.lane_w;
    const bool dimmed = track.muted || (ctx.any_solo && !track.solo);
    ctx.draw->AddRectFilled(ImVec2(ctx.lane_x, y), ImVec2(right, y + height),
                            ImGui::GetColorU32(dimmed ? ImGuiCol_ScrollbarBg : ImGuiCol_ChildBg));
    ctx.draw->AddLine(ImVec2(ctx.lane_x, y + height), ImVec2(right, y + height),
                      ImGui::GetColorU32(ImGuiCol_Border), Gui::Dpi::S(1.0F));

    ImGui::SetCursorScreenPos(ImVec2(ctx.lane_x, y));
    ImGui::SetNextItemAllowOverlap();
    const std::string lane_id = "###tl_lane_" + track.id;
    ImGui::InvisibleButton(lane_id.c_str(), ImVec2(std::max(1.0F, ctx.lane_w), height));
    const bool lane_hovered = ImGui::IsItemHovered();

    if (lane_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !DragActive()) {
        BeginBand(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
    }
    if (ImGui::BeginPopupContextItem(("##tl_lane_menu_" + track.id).c_str())) {
        LaneMenu(ctx, track, CursorFrame(ctx));
        ImGui::EndPopup();
    }

    for (const Doc::Clip& clip : track.clips)
        DrawClip(ctx, track, clip, y);
}

void BeginBand(float x, float y) {
    g_band.anchor = ImVec2(x, y);
    g_band.active = true;
}

void UpdateBand(Ctx& ctx) {
    if (!g_band.active) return;
    if (DragActive()) {
        g_band.active = false;
        return;
    }

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const float x0 = std::min(g_band.anchor.x, mouse.x);
    const float x1 = std::max(g_band.anchor.x, mouse.x);
    const float y0 =
        std::clamp(std::min(g_band.anchor.y, mouse.y), ctx.lanes_top, ctx.lanes_bottom);
    const float y1 =
        std::clamp(std::max(g_band.anchor.y, mouse.y), ctx.lanes_top, ctx.lanes_bottom);
    ctx.draw->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1),
                            ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
    ctx.draw->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), ImGui::GetColorU32(ImGuiCol_CheckMark));

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;
    g_band.active = false;
    std::vector<std::string> hit;
    for (const ClipRect& rect : ctx.clip_rects) {
        if (rect.x0 < x1 && x0 < rect.x1 && rect.y0 < y1 && y0 < rect.y1) hit.push_back(rect.id);
    }
    ctx.editor->SetSelection(std::move(hit));
}

}
