#include "gui_tl_internal.h"

#include "editor/preset_editor_state.h"
#include "editor/timeline_drag.h"
#include "editor/timeline_edits.h"
#include "editor/timeline_view.h"
#include "imgui.h"
#include "preset/doc/preset_document.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

struct Live {
    Editor::DragStart start = {};
    bool active = false;
    bool duplicated = false;
};

Live g_drag;

std::string TrackUnder(const Ctx& ctx, float y) {
    for (const TrackBand& band : ctx.bands) {
        if (y >= band.y0 && y < band.y1) return band.id;
    }
    return g_drag.start.track_id;
}

void DrawGuide(const Ctx& ctx, int frame) {
    const float x = FrameToX(ctx, frame);
    if (x < ctx.lane_x || x > ctx.lane_x + ctx.lane_w) return;
    const ImU32 accent = ImGui::GetColorU32(ImGuiCol_CheckMark);
    DashedVertical(ctx, x, ctx.lanes_top, ctx.lanes_bottom, accent);

    char badge[24];
    snprintf(badge, sizeof(badge), "%d", frame);
    const ImVec2 size = ImGui::CalcTextSize(badge);
    ctx.draw->AddRectFilled(ImVec2(x + 2.0F, ctx.lanes_top),
                            ImVec2(x + 6.0F + size.x, ctx.lanes_top + size.y + 2.0F),
                            ImGui::GetColorU32(ImGuiCol_PopupBg));
    ctx.draw->AddText(ImVec2(x + 4.0F, ctx.lanes_top + 1.0F), accent, badge);
}

void DrawRefusal(const Ctx& ctx) {
    const float x = FrameToX(ctx, g_drag.start.start);
    ctx.draw->AddCircleFilled(ImVec2(x, ctx.lanes_top + 6.0F), 4.0F,
                              ImGui::GetColorU32(ImVec4(1.0F, 0.45F, 0.45F, 0.9F)));
}

void JumpToEdge(const Ctx& ctx, int direction) {
    const std::vector<int> edges = Editor::EdgeFrames(*ctx.document);
    int best = ctx.status.frame;
    for (const int frame : edges) {
        if (direction > 0 && frame > ctx.status.frame) {
            best = frame;
            break;
        }
        if (direction < 0 && frame < ctx.status.frame) best = frame;
    }
    PostSeek(std::clamp(best, 0, std::max(0, ctx.length - 1)));
}

Editor::DragInput MakeInput(const Ctx& ctx) {
    const ImGuiIO& io = ImGui::GetIO();
    Editor::DragInput input;
    input.cursor_frame = CursorFrame(ctx);
    input.track_id = TrackUnder(ctx, io.MousePos.y);
    input.playhead = ctx.status.frame;
    input.px_per_frame = ctx.editor->GetView().px_per_frame;
    input.snap = ctx.editor->GetView().snap && !io.KeyAlt;
    return input;
}

bool StartDuplicate(Ctx& ctx, const Editor::DragInput& input, const Editor::DragResult& result) {
    if (result.start == g_drag.start.start && result.track_id == g_drag.start.track_id) return true;
    if (Editor::OverlapsSelfCopy(*ctx.document, result.track_id, g_drag.start.clip_id, result.start,
                                 result.end)) {
        DrawRefusal(ctx);
        return true;
    }

    const std::string source = g_drag.start.clip_id;
    const std::string copy_id = Editor::UniqueClipId(*ctx.document, source);
    const std::string track_id = result.track_id;
    const int start = result.start;
    ApplyEdit(ctx, [source, track_id, copy_id, start](Doc::Document& document) {
        return Editor::DuplicateClipTo(document, source, track_id, copy_id, start);
    });

    g_drag.duplicated = true;
    g_drag.start.clip_id = copy_id;
    g_drag.start.track_id = track_id;
    g_drag.start.start = start;
    g_drag.start.end = result.end;
    g_drag.start.grab_frame = input.cursor_frame;
    return true;
}

void HandleTransportKeys(Ctx& ctx) {
    const ImGuiIO& io = ImGui::GetIO();
    const int last = std::max(0, ctx.length - 1);
    const int step = io.KeyShift ? 100 : 1;
    if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Space, false)) PostPaused(ctx.status.playing);
    if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) PostSeek(0);
    if (ImGui::IsKeyPressed(ImGuiKey_End, false)) PostSeek(last);
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        PostSeek(std::clamp(ctx.status.frame - step, 0, last));
    }
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        PostSeek(std::clamp(ctx.status.frame + step, 0, last));
    }
    if (io.KeyCtrl) return;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket, false)) JumpToEdge(ctx, -1);
    if (ImGui::IsKeyPressed(ImGuiKey_RightBracket, false)) JumpToEdge(ctx, 1);
}

void HandleClipboardKeys(Ctx& ctx) {
    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) ctx.editor->Undo();
    if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) ctx.editor->Redo();
    if (ImGui::IsKeyPressed(ImGuiKey_C, false)) CopySelection(ctx, false);
    if (ImGui::IsKeyPressed(ImGuiKey_X, false)) CopySelection(ctx, true);
    if (ImGui::IsKeyPressed(ImGuiKey_V, false)) PasteAt(ctx, ctx.status.frame);
    if (ImGui::IsKeyPressed(ImGuiKey_0, false)) {
        ctx.editor->MutView().px_per_frame = Editor::FitZoom(ctx.length, ctx.lane_w);
        ctx.editor->MutView().scroll = 0.0;
    }
    if (!ImGui::IsKeyPressed(ImGuiKey_D, false)) return;
    const std::vector<std::string> ids = ctx.editor->Selection();
    ApplyEdit(ctx, [ids](Doc::Document& document) {
        return !Editor::DuplicateClips(document, ids).empty();
    });
}

void ToggleTracksOfSelection(Ctx& ctx, bool lock) {
    const std::vector<std::string> ids = ctx.editor->Selection();
    ApplyEdit(ctx, [ids, lock](Doc::Document& document) {
        bool changed = false;
        for (const std::string& id : ids) {
            const Editor::ClipRef ref = Editor::FindClip(document, id);
            if (!ref.Valid()) continue;
            const Doc::Track& track = document.tracks[(std::size_t)ref.track];
            changed = (lock ? Editor::SetTrackLocked(document, track.id, !track.locked)
                            : Editor::SetTrackMuted(document, track.id, !track.muted)) ||
                      changed;
        }
        return changed;
    });
}

void HandleClipKeys(Ctx& ctx) {
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) DeleteSelection(ctx);
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        CancelDrag();
        ctx.editor->ClearSelection();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) && !ctx.editor->Selection().empty()) {
        ctx.editor->PostRequest(Editor::Request{.kind = Editor::RequestKind::ClipProperties,
                                                .clip_id = ctx.editor->Selection().front()});
    }
    if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        const std::vector<std::string> ids = ctx.editor->Selection();
        const int playhead = ctx.status.frame;
        ApplyEdit(ctx, [ids, playhead](Doc::Document& document) {
            bool split = false;
            for (const std::string& id : ids)
                split = Editor::SplitClip(document, id, playhead) || split;
            return split;
        });
    }
    if (ImGui::IsKeyPressed(ImGuiKey_M, false)) ToggleTracksOfSelection(ctx, false);
    if (ImGui::IsKeyPressed(ImGuiKey_L, false)) ToggleTracksOfSelection(ctx, true);
}

void ApplyResult(Ctx& ctx, const Editor::DragResult& result) {
    const std::string id = g_drag.start.clip_id;
    const std::string track_id = result.track_id;
    const int start = result.start;
    const std::optional<int> end = result.end;
    if (g_drag.start.mode == Editor::DragMode::Move) {
        ApplyEdit(ctx, [id, track_id, start](Doc::Document& document) {
            return Editor::MoveClipToTrack(document, id, track_id, start);
        });
        return;
    }
    ApplyEdit(ctx, [id, start, end](Doc::Document& document) {
        return Editor::ResizeClip(document, id, start, end);
    });
}

}

bool DragActive() {
    return g_drag.active;
}

void CancelDrag() {
    g_drag = Live{};
}

void BeginDrag(Ctx& ctx, const Doc::Track& track, const Doc::Clip& clip, Editor::DragMode mode) {
    if (g_drag.active) return;
    const double pressed = XToFrame(ctx, ImGui::GetIO().MouseClickedPos[0].x);
    g_drag.active = true;
    g_drag.duplicated = false;
    g_drag.start = Editor::DragStart{
        .mode = mode,
        .clip_id = clip.id,
        .track_id = track.id,
        .start = clip.start,
        .end = clip.end,
        .grab_frame = std::clamp((int)std::lround(pressed), 0, std::max(0, ctx.length - 1)),
        .duplicate = ImGui::GetIO().KeyCtrl};
    ctx.editor->BeginGesture();
}

void UpdateDrag(Ctx& ctx) {
    if (!g_drag.active) return;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ctx.editor->EndGesture();
        g_drag = Live{};
        return;
    }
    if (!Editor::FindClip(*ctx.document, g_drag.start.clip_id).Valid()) {
        ctx.editor->EndGesture();
        g_drag = Live{};
        return;
    }
    const Editor::DragInput input = MakeInput(ctx);
    const Editor::DragResult result = Editor::ResolveDrag(*ctx.document, g_drag.start, input);
    if (result.snapped) DrawGuide(ctx, result.snap_frame);
    if (!result.allowed) {
        DrawRefusal(ctx);
        return;
    }
    if (g_drag.start.duplicate && !g_drag.duplicated && StartDuplicate(ctx, input, result)) return;
    ApplyResult(ctx, result);
}

void HandleShortcuts(Ctx& ctx) {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
        !ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
        return;
    }
    HandleTransportKeys(ctx);
    if (io.KeyCtrl) {
        HandleClipboardKeys(ctx);
        return;
    }
    HandleClipKeys(ctx);
}

}
