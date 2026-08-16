#include "gui_tl_internal.h"
#include "gui_tl_modals.h"

#include "editor/clip_problems.h"
#include "editor/clip_summary.h"
#include "editor/preset_editor_state.h"
#include "editor/timeline_drag.h"
#include "editor/timeline_edits.h"
#include "editor/timeline_lanes.h"
#include "imgui.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

constexpr float kEventW = 4.0F;
constexpr float kEventHitW = 12.0F;
constexpr float kGateStripeH = 3.0F;
constexpr float kProblemBarW = 4.0F;
constexpr ImU32 kProblemError = IM_COL32(232, 96, 88, 255);
constexpr ImU32 kProblemWarning = IM_COL32(232, 176, 72, 255);

struct Box {
    float x0 = 0.0F;
    float x1 = 0.0F;
    float y0 = 0.0F;
    float y1 = 0.0F;
};

Box ClipBox(const Ctx& ctx, const Doc::Track& track, const Doc::Clip& clip, float y) {
    const int lane = Editor::LaneOf(track, clip);
    const float row = RowHeight();
    Box box;
    box.y0 =
        lane == 0 ? y + ((row - kClipH) * 0.5F) : y + row + ((float)(lane - 1) * kSubLaneH) + 1.0F;
    box.y1 = box.y0 + (lane == 0 ? kClipH : kSubLaneH - 2.0F);
    box.x0 = FrameToX(ctx, clip.start);
    box.x1 =
        Editor::IsEvent(clip) ? box.x0 + kEventW : FrameToX(ctx, Editor::ClipEnd(clip, ctx.length));
    return box;
}

void DrawBody(const Ctx& ctx, const Doc::Track& track, const Doc::Clip& clip, const Box& box,
              const Box& visible) {
    const bool dimmed = clip.muted || track.muted || (ctx.any_solo && !track.solo);
    ImU32 fill = CommandColor(Doc::TypeOf(clip.command));
    if (dimmed) fill = (fill & 0x00FFFFFFU) | 0x66000000U;
    ctx.draw->AddRectFilled(ImVec2(visible.x0, box.y0), ImVec2(visible.x1, box.y1), fill);

    if (track.locked) {
        ctx.draw->AddRectFilled(ImVec2(visible.x0, box.y0), ImVec2(visible.x0 + 3.0F, box.y1),
                                ImGui::GetColorU32(ImGuiCol_TextDisabled));
    }
    const Editor::ClipProblems problems = Editor::ProblemsForClip(CurrentProblems(), clip.id);
    if (problems.Any()) {
        ctx.draw->AddRectFilled(ImVec2(visible.x0, box.y0),
                                ImVec2(visible.x0 + kProblemBarW, box.y1),
                                problems.Failing() ? kProblemError : kProblemWarning);
    }
    if (clip.when.has_value()) {
        ctx.draw->AddRectFilled(ImVec2(visible.x0, box.y0),
                                ImVec2(visible.x1, box.y0 + kGateStripeH),
                                ImGui::GetColorU32(ImGuiCol_CheckMark));
    }
    if (!clip.end.has_value() && !Editor::IsEvent(clip)) {
        ctx.draw->AddText(ImVec2(visible.x1 - 10.0F, box.y0 + 2.0F),
                          ImGui::GetColorU32(ImGuiCol_Text), ">");
    }

    for (const Doc::Key& key : clip.keys) {
        const float kx = FrameToX(ctx, clip.start + key.at);
        if (kx < visible.x0 || kx > visible.x1) continue;
        const float ky = (box.y0 + box.y1) * 0.5F;
        ctx.draw->AddQuadFilled(ImVec2(kx, ky - 4.0F), ImVec2(kx + 4.0F, ky), ImVec2(kx, ky + 4.0F),
                                ImVec2(kx - 4.0F, ky), ImGui::GetColorU32(ImGuiCol_Text));
    }

    const std::string label =
        Ellipsized(Editor::ClipSummary(clip, track.target), visible.x1 - visible.x0 - 8.0F);
    ctx.draw->AddText(ImVec2(visible.x0 + 5.0F, box.y0 + 3.0F),
                      ImGui::GetColorU32(ImGuiCol_WindowBg), label.c_str());

    if (!ctx.editor->IsSelected(clip.id)) return;
    ctx.draw->AddRect(ImVec2(visible.x0, box.y0), ImVec2(visible.x1, box.y1),
                      ImGui::GetColorU32(ImGuiCol_Text), 0.0F, 0, 2.0F);
}

std::string ProblemLines(const Doc::Clip& clip) {
    const Editor::ClipProblems problems = Editor::ProblemsForClip(CurrentProblems(), clip.id);
    std::string out;
    for (const std::string& message : problems.messages)
        out += (problems.Failing() ? "\nerror: " : "\nwarning: ") + message;
    return out;
}

void HandleClick(Ctx& ctx, const Doc::Track& track, const Doc::Clip& clip, const Box& visible) {
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s\nframes %d..%d%s%s\nclick selects, double-click opens properties",
                          Editor::ClipSummary(clip, track.target).c_str(), clip.start,
                          Editor::ClipEnd(clip, ctx.length) - 1, clip.muted ? "\nmuted" : "",
                          ProblemLines(clip).c_str());
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        ctx.editor->Select(clip.id);
        ctx.editor->PostRequest(
            Editor::Request{.kind = Editor::RequestKind::ClipProperties, .clip_id = clip.id});
        return;
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        if (io.KeyShift) {
            ctx.editor->ExtendSelection(clip.id);
        } else if (io.KeyCtrl) {
            ctx.editor->ToggleSelection(clip.id);
        } else if (!ctx.editor->IsSelected(clip.id)) {
            ctx.editor->Select(clip.id);
        }
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !ctx.editor->IsSelected(clip.id)) {
        ctx.editor->Select(clip.id);
    }

    const Editor::Zone zone = Editor::ZoneAt(visible.x0, visible.x1, io.MousePos.x);
    if (ImGui::IsItemHovered() && zone != Editor::Zone::Body && zone != Editor::Zone::None) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (!ImGui::IsItemActive() || !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) return;
    if (Editor::TrackLocked(*ctx.document, track.id)) return;

    Editor::DragMode mode = Editor::DragMode::Move;
    if (zone == Editor::Zone::LeftHandle) mode = Editor::DragMode::ResizeLeft;
    if (zone == Editor::Zone::RightHandle) mode = Editor::DragMode::ResizeRight;
    BeginDrag(ctx, track, clip, mode);
}

}

void ClipMenuItems(Ctx& ctx, const Doc::Clip& clip) {
    const std::string id = clip.id;
    const int playhead = ctx.status.frame;
    if (ImGui::MenuItem("Properties...", "Enter")) {
        ctx.editor->PostRequest(
            Editor::Request{.kind = Editor::RequestKind::ClipProperties, .clip_id = id});
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Cut", "Ctrl+X")) CopySelection(ctx, true);
    if (ImGui::MenuItem("Copy", "Ctrl+C")) CopySelection(ctx, false);
    if (ImGui::MenuItem("Paste at playhead", "Ctrl+V", false, !ctx.editor->Clipboard().empty())) {
        PasteAt(ctx, playhead);
    }
    if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
        const std::vector<std::string> ids = ctx.editor->Selection();
        ApplyEdit(ctx, [ids](Doc::Document& document) {
            return !Editor::DuplicateClips(document, ids).empty();
        });
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Split at playhead", "S")) {
        ApplyEdit(ctx, [id, playhead](Doc::Document& document) {
            return Editor::SplitClip(document, id, playhead);
        });
    }
    if (ImGui::MenuItem("Trim start to playhead")) {
        ApplyEdit(ctx, [id, playhead](Doc::Document& document) {
            return Editor::TrimStart(document, id, playhead);
        });
    }
    if (ImGui::MenuItem("Trim end to playhead")) {
        ApplyEdit(ctx, [id, playhead](Doc::Document& document) {
            return Editor::TrimEnd(document, id, playhead);
        });
    }
    if (ImGui::MenuItem("Make open-ended")) {
        ApplyEdit(ctx,
                  [id](Doc::Document& document) { return Editor::MakeOpenEnded(document, id); });
    }
    ImGui::MenuItem("Add key at playhead", "K", false, false);
    ImGui::Separator();
    const bool muted = clip.muted;
    if (ImGui::MenuItem("Mute clip", nullptr, muted)) {
        ApplyEdit(ctx, [id, muted](Doc::Document& document) {
            return Editor::SetClipMuted(document, id, !muted);
        });
    }
    if (ImGui::MenuItem("Delete", "Del")) DeleteSelection(ctx);
}

void DeleteSelection(Ctx& ctx) {
    const std::vector<std::string> ids = ctx.editor->Selection();
    if (ids.empty()) return;
    ApplyEdit(ctx, [ids](Doc::Document& document) { return Editor::DeleteClips(document, ids); });
}

void CopySelection(Ctx& ctx, bool cut) {
    const std::vector<std::string> ids = ctx.editor->Selection();
    if (ids.empty()) return;
    ctx.editor->SetClipboard(Editor::CopyClips(*ctx.document, ids));
    if (cut) DeleteSelection(ctx);
}

void PasteAt(Ctx& ctx, int frame) {
    const std::vector<Editor::ClipboardClip> clips = ctx.editor->Clipboard();
    if (clips.empty()) return;
    ApplyEdit(ctx, [clips, frame](Doc::Document& document) {
        return !Editor::PasteClips(document, clips, frame).empty();
    });
}

void DrawClip(Ctx& ctx, const Doc::Track& track, const Doc::Clip& clip, float y) {
    const Box box = ClipBox(ctx, track, clip, y);
    const float right = ctx.lane_x + ctx.lane_w;
    if (box.x1 < ctx.lane_x || box.x0 > right) return;

    Box visible = box;
    visible.x0 = std::max(box.x0, ctx.lane_x);
    visible.x1 = std::min(std::max(box.x1, visible.x0 + kEventW), right);

    DrawBody(ctx, track, clip, box, visible);
    ctx.clip_rects.push_back(
        ClipRect{.id = clip.id, .x0 = visible.x0, .x1 = visible.x1, .y0 = box.y0, .y1 = box.y1});

    ImGui::SetCursorScreenPos(ImVec2(visible.x0, box.y0));
    ImGui::SetNextItemAllowOverlap();
    const std::string clip_id = "###tl_clip_" + clip.id;
    const float hit_w = Editor::IsEvent(clip) ? kEventHitW : (visible.x1 - visible.x0);
    ImGui::InvisibleButton(clip_id.c_str(), ImVec2(hit_w, box.y1 - box.y0));
    HandleClick(ctx, track, clip, visible);

    if (!ImGui::BeginPopupContextItem(("##tl_clip_menu_" + clip.id).c_str())) return;
    ClipMenuItems(ctx, clip);
    ImGui::EndPopup();
}

}
