#include "gui_tl_internal.h"

#include "editor/preset_editor_state.h"
#include "editor/export_range.h"
#include "editor/timeline_edits.h"
#include "editor/timeline_view.h"
#include "gui/gui_style.h"
#include "imgui.h"
#include "preset/doc/preset_document.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

constexpr float kMarkerHalf = 6.0F;

char g_marker_name[96] = {};
int g_marker_menu = -1;
int g_range_anchor = -1;

void DrawTicks(const Ctx& ctx, float top) {
    const Editor::View& view = ctx.editor->GetView();
    const int step = Editor::RulerStep(view.px_per_frame);
    const int fps = std::max(1, ctx.document->fps);
    const ImU32 line = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32 text = ImGui::GetColorU32(ImGuiCol_TextDisabled);

    const auto first = (int)std::floor(view.scroll / step) * step;
    const auto last = (int)std::ceil(XToFrame(ctx, ctx.lane_x + ctx.lane_w));
    for (int frame = std::max(0, first); frame <= last; frame += step) {
        const float x = FrameToX(ctx, frame);
        if (x < ctx.lane_x - 1.0F || x > ctx.lane_x + ctx.lane_w) continue;
        ctx.draw->AddLine(ImVec2(x, top), ImVec2(x, top + kRulerH), line, 1.0F);
        char frames[24];
        snprintf(frames, sizeof(frames), "%d", frame);
        ctx.draw->AddText(ImVec2(x + 3.0F, top + 1.0F), text, frames);
        char seconds[24];
        snprintf(seconds, sizeof(seconds), "%.2fs", (double)frame / (double)fps);
        ctx.draw->AddText(ImVec2(x + 3.0F, top + 16.0F), text, seconds);
    }
}

void DrawEndLine(Ctx& ctx, float top) {
    const float x = FrameToX(ctx, ctx.length);
    const float right = ctx.lane_x + ctx.lane_w;
    if (x < right) {
        ctx.draw->AddRectFilled(ImVec2(std::max(x, ctx.lane_x), top),
                                ImVec2(right, ctx.lanes_bottom),
                                ImGui::GetColorU32(ImGuiCol_FrameBg, 0.55F));
    }
    if (x < ctx.lane_x || x > right) return;

    const ImU32 color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    DashedVertical(ctx, x, top, ctx.lanes_bottom, color);
    char label[32];
    snprintf(label, sizeof(label), "end %d", ctx.length);
    ctx.draw->AddText(ImVec2(x + 4.0F, ctx.lanes_bottom - 16.0F), color, label);

    ImGui::SetCursorScreenPos(ImVec2(x - 4.0F, top));
    ImGui::InvisibleButton("###tl_end_line", ImVec2(9.0F, ctx.lanes_bottom - top));
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (ImGui::IsItemActivated()) ctx.editor->BeginGesture();
    if (ImGui::IsItemDeactivated()) ctx.editor->EndGesture();
    if (!ImGui::IsItemActive()) return;
    const int wanted = std::max(1, CursorFrame(ctx) + 1);
    ApplyEdit(ctx,
              [wanted](Doc::Document& document) { return Editor::SetLength(document, wanted); });
}

void MarkerPopup(Ctx& ctx) {
    if (!ImGui::BeginPopup("##tl_marker_menu")) return;
    const int index = g_marker_menu;
    if (index >= 0 && std::cmp_less(index, ctx.document->markers.size())) {
        ImGui::SetNextItemWidth(180.0F);
        ImGui::InputText("###tl_marker_name", g_marker_name, sizeof(g_marker_name));
        if (ImGui::MenuItem("Rename marker")) {
            const std::string label = g_marker_name;
            ApplyEdit(ctx, [index, label](Doc::Document& document) {
                return Editor::RenameMarker(document, index, label);
            });
        }
        if (ImGui::MenuItem("Delete marker")) {
            ApplyEdit(ctx, [index](Doc::Document& document) {
                return Editor::DeleteMarker(document, index);
            });
        }
    }
    ImGui::EndPopup();
}

int MarkerHit(const Ctx& ctx, float mouse_x) {
    for (std::size_t i = 0; i < ctx.document->markers.size(); i++) {
        const float x = FrameToX(ctx, ctx.document->markers[i].frame);
        if (std::fabs(mouse_x - x) <= kMarkerHalf) return (int)i;
    }
    return -1;
}

void DrawMarkers(Ctx& ctx, float top) {
    const ImU32 accent = ImGui::GetColorU32(ImGuiCol_CheckMark);
    const float base = top + kRulerH;
    for (std::size_t i = 0; i < ctx.document->markers.size(); i++) {
        const Doc::Marker& marker = ctx.document->markers[i];
        const float x = FrameToX(ctx, marker.frame);
        if (x < ctx.lane_x - kMarkerHalf || x > ctx.lane_x + ctx.lane_w) continue;
        ctx.draw->AddTriangleFilled(ImVec2(x - kMarkerHalf, base - 9.0F),
                                    ImVec2(x + kMarkerHalf, base - 9.0F), ImVec2(x, base - 1.0F),
                                    accent);
    }
}

void DragMarker(Ctx& ctx, int index) {
    if (index < 0) return;
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const int frame = CursorFrame(ctx);
        ApplyEdit(ctx, [index, frame](Doc::Document& document) {
            return Editor::MoveMarker(document, index, frame);
        });
    }
}

void DrawExportRange(const Ctx& ctx, float top) {
    const Editor::ExportRange range = ctx.editor->GetView().export_range;
    if (!range.active) return;
    const float x0 = std::max(FrameToX(ctx, range.start), ctx.lane_x);
    const float x1 = std::min(FrameToX(ctx, range.end), ctx.lane_x + ctx.lane_w);
    if (x1 <= ctx.lane_x || x0 >= ctx.lane_x + ctx.lane_w) return;
    const ImU32 accent = ImGui::GetColorU32(ImGuiCol_CheckMark);
    const float y = top + kRulerH - 3.0F;
    ctx.draw->AddRectFilled(ImVec2(x0, y - 1.0F), ImVec2(x1, y + 1.0F), accent);
    ctx.draw->AddRectFilled(ImVec2(x0, y - 6.0F), ImVec2(x0 + 2.0F, y + 2.0F), accent);
    ctx.draw->AddRectFilled(ImVec2(x1 - 2.0F, y - 6.0F), ImVec2(x1, y + 2.0F), accent);
    char label[48];
    snprintf(label, sizeof(label), "export %d..%d", range.start, range.end - 1);
    const ImVec2 size = ImGui::CalcTextSize(label);
    const float text_x = ctx.lane_x - size.x - 8.0F;
    if (text_x < ctx.header_x + 4.0F) return;
    ctx.draw->AddText(ImVec2(text_x, y - size.y - 2.0F), accent, label);
}

bool UpdateExportRange(Ctx& ctx, bool hovered) {
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsItemActivated() && io.KeyShift && hovered) g_range_anchor = CursorFrame(ctx);
    if (!ImGui::IsItemActive()) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) g_range_anchor = -1;
        return false;
    }
    if (g_range_anchor < 0) return false;
    ctx.editor->MutView().export_range =
        Editor::RangeFromDrag(g_range_anchor, CursorFrame(ctx), ctx.length);
    return true;
}

void RulerInput(Ctx& ctx, float top) {
    ImGui::SetCursorScreenPos(ImVec2(ctx.lane_x, top));
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("###tl_ruler", ImVec2(std::max(1.0F, ctx.lane_w), kRulerH));
    const bool hovered = ImGui::IsItemHovered();
    const int marker = hovered ? MarkerHit(ctx, ImGui::GetIO().MousePos.x) : -1;

    if (marker >= 0 && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const Doc::Marker& hit = ctx.document->markers[(std::size_t)marker];
        ImGui::SetTooltip("marker '%s' at frame %d\ndrag to move, right-click to rename or delete",
                          hit.label.c_str(), hit.frame);
    }
    if (marker >= 0 && ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        g_marker_menu = marker;
        const std::string& label = ctx.document->markers[(std::size_t)marker].label;
        snprintf(g_marker_name, sizeof(g_marker_name), "%s", label.c_str());
        ImGui::OpenPopup("##tl_marker_menu");
    }
    MarkerPopup(ctx);

    if (marker < 0 && ImGui::IsItemClicked(ImGuiMouseButton_Right) &&
        ctx.editor->GetView().export_range.active) {
        ctx.editor->MutView().export_range = Editor::ExportRange{};
    }
    if (UpdateExportRange(ctx, hovered)) return;
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && marker < 0) {
        const int frame = CursorFrame(ctx);
        ApplyEdit(ctx, [frame](Doc::Document& document) {
            return Editor::AddMarker(document, frame, "marker " + std::to_string(frame));
        });
        return;
    }
    if (!ImGui::IsItemActive()) return;
    if (marker >= 0) {
        DragMarker(ctx, marker);
        return;
    }
    PostSeek(CursorFrame(ctx));
}

void HandleWheel(Ctx& ctx) {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.MouseWheel == 0.0F) return;
    if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) return;

    Editor::View& view = ctx.editor->MutView();
    if (io.KeyShift) {
        view.track_scroll -= io.MouseWheel * kTrackH;
        return;
    }
    if (io.KeyCtrl) {
        const double anchor = XToFrame(ctx, io.MousePos.x);
        view.px_per_frame = Editor::ClampZoom(view.px_per_frame * std::pow(1.2, io.MouseWheel));
        view.scroll = anchor - ((double)(io.MousePos.x - ctx.lane_x) / view.px_per_frame);
    } else {
        view.scroll -= (double)io.MouseWheel * (60.0 / view.px_per_frame);
    }
    view.scroll = Editor::ClampScroll(view.scroll, ctx.length, view.px_per_frame, ctx.lane_w);
}

}

void DrawRuler(Ctx& ctx) {
    const float top = ctx.lanes_top - kRulerH;
    ctx.draw->AddRectFilled(ImVec2(ctx.header_x, top),
                            ImVec2(ctx.lane_x + ctx.lane_w, ctx.lanes_top),
                            ImGui::GetColorU32(ImGuiCol_TitleBg));
    Gui::PushMonoFont();
    DrawTicks(ctx, top);
    DrawEndLine(ctx, top);
    ImGui::PopFont();
    DrawMarkers(ctx, top);
    DrawExportRange(ctx, top);
    RulerInput(ctx, top);
    HandleWheel(ctx);
}

void DrawPlayhead(const Ctx& ctx) {
    const float x = FrameToX(ctx, ctx.status.frame);
    if (x < ctx.lane_x || x > ctx.lane_x + ctx.lane_w) return;
    const float top = ctx.lanes_top - kRulerH;
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
    ctx.draw->AddRectFilled(ImVec2(x - 1.0F, top), ImVec2(x + 1.0F, ctx.lanes_bottom), color);
    ctx.draw->AddTriangleFilled(ImVec2(x - 5.0F, top), ImVec2(x + 5.0F, top), ImVec2(x, top + 7.0F),
                                color);

    char badge[24];
    snprintf(badge, sizeof(badge), "%d", ctx.status.frame);
    const ImVec2 size = ImGui::CalcTextSize(badge);
    ctx.draw->AddRectFilled(ImVec2(x + 3.0F, top), ImVec2(x + 7.0F + size.x, top + size.y + 2.0F),
                            ImGui::GetColorU32(ImGuiCol_PopupBg));
    ctx.draw->AddText(ImVec2(x + 5.0F, top + 1.0F), color, badge);
}

}
