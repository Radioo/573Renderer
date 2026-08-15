#include "gui_tl_internal.h"

#include "editor/preset_editor_state.h"
#include "editor/timeline_edits.h"
#include "editor/timeline_view.h"
#include "gui/gui_icons.h"
#include "gui/gui_style.h"
#include "imgui.h"
#include "preset/doc/preset_document.h"
#include "state/app_state.h"
#include "state/preset_commands.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace Panels::Timeline {

namespace {

constexpr float kZoomSliderW = 120.0F;

bool TransportButton(const char* label, const char* tip) {
    const bool pressed = ImGui::Button(label, ImVec2(32, 0));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    return pressed;
}

void Step(const Ctx& ctx, int delta) {
    PostSeek(std::clamp(ctx.status.frame + delta, 0, std::max(0, ctx.length - 1)));
}

void JumpEdge(const Ctx& ctx, int direction) {
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

std::string SelectedTrack(const Ctx& ctx) {
    if (ctx.editor->Selection().empty()) return {};
    const Editor::ClipRef ref = Editor::FindClip(*ctx.document, ctx.editor->Selection().front());
    if (!ref.Valid()) return {};
    return ctx.document->tracks[(std::size_t)ref.track].id;
}

void DrawZoom(Ctx& ctx) {
    Editor::View& view = ctx.editor->MutView();
    auto zoom = (float)view.px_per_frame;
    ImGui::SetNextItemWidth(kZoomSliderW);
    if (ImGui::SliderFloat("###tl_zoom", &zoom, (float)Editor::kZoomMin, (float)Editor::kZoomMax,
                           "%.2f px/frame", ImGuiSliderFlags_Logarithmic)) {
        view.px_per_frame = Editor::ClampZoom((double)zoom);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("How many pixels one document frame takes.\nCtrl+wheel over the tracks "
                          "zooms about the cursor.");
    }
    ImGui::SameLine(0.0F, 4.0F);
    if (ImGui::Button("fit###tl_fit")) {
        view.px_per_frame = Editor::FitZoom(ctx.length, ctx.lane_w > 1.0F ? ctx.lane_w : 800.0F);
        view.scroll = 0.0;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Fit the whole document in the view (Ctrl+0).");
}

void DrawButtons(Ctx& ctx) {
    const int last = std::max(0, ctx.length - 1);

    if (TransportButton(ICON_JUMP_BACK "###tl_jump_start", "Jump to frame 0 (Home).")) PostSeek(0);
    ImGui::SameLine(0.0F, 3.0F);
    if (TransportButton(ICON_STEP_BACK "###tl_step_back",
                        "Step back one frame (Left, Shift x100).")) {
        Step(ctx, -1);
    }
    ImGui::SameLine(0.0F, 3.0F);
    const char* play_icon = ctx.status.playing ? ICON_PAUSE "###tl_play" : ICON_PLAY "###tl_play";
    if (TransportButton(play_icon, "Play / pause (Space).")) PostPaused(ctx.status.playing);
    ImGui::SameLine(0.0F, 3.0F);
    if (TransportButton(ICON_STEP_FWD "###tl_step_fwd", "Step forward one frame (Right).")) {
        Step(ctx, 1);
    }
    ImGui::SameLine(0.0F, 3.0F);
    if (TransportButton(ICON_JUMP_FWD "###tl_jump_end", "Jump to the last frame (End).")) {
        PostSeek(last);
    }

    ImGui::SameLine(0.0F, 14.0F);
    Gui::PushMonoFont();
    ImGui::AlignTextToFramePadding();
    const int fps = std::max(1, ctx.status.fps);
    ImGui::Text("%d / %d", ctx.status.frame, ctx.length);
    ImGui::SameLine(0.0F, 10.0F);
    ImGui::TextDisabled("%.2f s", (double)ctx.status.frame / (double)fps);
    ImGui::PopFont();
}

void DrawToggles(Ctx& ctx) {
    ImGui::SameLine(0.0F, 12.0F);
    bool loop = ctx.status.loop;
    if (ImGui::Checkbox("loop###tl_loop", &loop)) {
        App::Global().PostCommand(PresetCmd::Wrap(PresetCmd::SetLoop{.loop = loop}));
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("On: the frame after the last one is 0.\nOff: playback pauses on the "
                          "last frame and stays there.");
    }

    ImGui::SameLine(0.0F, 8.0F);
    bool snap = ctx.editor->GetView().snap;
    if (ImGui::Checkbox("snap###tl_snap", &snap)) ctx.editor->MutView().snap = snap;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Snap dragged clip edges to the playhead, other clips, tween keys,\n"
                          "the document bounds and the ruler ticks. Hold Alt to drag freely.");
    }

    ImGui::SameLine(0.0F, 12.0F);
    if (ImGui::Button("+ Command###tl_add_command")) {
        ctx.editor->PostRequest(Editor::Request{.kind = Editor::RequestKind::AddCommand,
                                                .track_id = SelectedTrack(ctx),
                                                .frame = ctx.status.frame});
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add a command clip on the selected track at the playhead.\n"
                          "The command palette arrives with the clip properties modal.");
    }
    ImGui::SameLine(0.0F, 4.0F);
    if (ImGui::Button("+ Track###tl_add_track")) {
        ctx.editor->PostRequest(
            Editor::Request{.kind = Editor::RequestKind::AddTrack, .track_id = SelectedTrack(ctx)});
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add a track. The Add track modal arrives with the command palette.");
    }
}

void DrawEdgeJumps(Ctx& ctx) {
    if (ImGui::Button("[###tl_prev_edge")) JumpEdge(ctx, -1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Jump to the previous clip edge, key or marker.");
    ImGui::SameLine(0.0F, 3.0F);
    if (ImGui::Button("]###tl_next_edge")) JumpEdge(ctx, 1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Jump to the next clip edge, key or marker.");
    ImGui::SameLine(0.0F, 12.0F);
}

std::string DocumentBadge(const Ctx& ctx) {
    return ctx.document->name + (ctx.editor->Dirty() ? " *" : "");
}

float TailWidth(const Ctx& ctx) {
    const float pad = ImGui::GetStyle().FramePadding.x * 2.0F;
    float width =
        ImGui::CalcTextSize("[").x + pad + 3.0F + ImGui::CalcTextSize("]").x + pad + 12.0F;
    width += kZoomSliderW;
    width += 4.0F + ImGui::CalcTextSize("fit").x + pad;
    width += 12.0F + ImGui::CalcTextSize("undo 000").x;
    width += 12.0F + ImGui::CalcTextSize(DocumentBadge(ctx).c_str()).x + pad;
    return width;
}

void DrawTail(Ctx& ctx) {
    DrawEdgeJumps(ctx);
    DrawZoom(ctx);

    ImGui::SameLine(0.0F, 12.0F);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("undo %d", ctx.editor->UndoDepth());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Undo entries held (Ctrl+Z / Ctrl+Y). One drag is one entry.");
    }

    ImGui::SameLine(0.0F, 12.0F);
    if (ImGui::SmallButton((DocumentBadge(ctx) + "###tl_doc_badge").c_str())) {
        ctx.editor->PostRequest(Editor::Request{.kind = Editor::RequestKind::DocumentProperties});
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s - the loaded document.%s", ctx.document->id.c_str(),
                          ctx.editor->Dirty() ? "\nIt has unsaved changes." : "");
    }
}

void PlaceTail(const Ctx& ctx) {
    const float right = ImGui::GetWindowContentRegionMax().x;
    const float used = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x;
    const float tail = TailWidth(ctx);
    const float at = std::max(0.0F, right - tail);
    if (used + 12.0F <= at) {
        ImGui::SameLine(at);
        return;
    }
    ImGui::SetCursorPosX(at);
}

}

void DrawTransport(Ctx& ctx) {
    DrawButtons(ctx);
    DrawToggles(ctx);
    PlaceTail(ctx);
    DrawTail(ctx);
}

}
