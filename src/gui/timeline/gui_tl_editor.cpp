#include "gui_timeline_editor.h"
#include "gui_tl_internal.h"
#include "gui_tl_modals.h"

#include "editor/preset_editor_state.h"
#include "editor/timeline_edits.h"
#include "editor/timeline_lanes.h"
#include "editor/timeline_view.h"
#include "imgui.h"
#include "preset/doc/preset_document.h"
#include "state/app_state.h"
#include "state/preset_commands.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

unsigned g_published = 0;

void DrawScrollBar(Ctx& ctx) {
    Editor::View& view = ctx.editor->MutView();
    const float y = ctx.lanes_bottom + 2.0F;
    ImGui::SetCursorScreenPos(ImVec2(ctx.lane_x, y));
    ImGui::InvisibleButton("###tl_scroll", ImVec2(std::max(1.0F, ctx.lane_w), kScrollBarH));

    const float track_x1 = ctx.lane_x + ctx.lane_w;
    ctx.draw->AddRectFilled(ImVec2(ctx.lane_x, y), ImVec2(track_x1, y + kScrollBarH),
                            ImGui::GetColorU32(ImGuiCol_ScrollbarBg));

    const double visible = (double)ctx.lane_w / std::max(1.0e-6, view.px_per_frame);
    const double span = std::max(1.0, (double)ctx.length);
    const float thumb_x0 = ctx.lane_x + ((float)(view.scroll / span) * ctx.lane_w);
    const float thumb_x1 =
        ctx.lane_x + ((float)(std::min(span, view.scroll + visible) / span) * ctx.lane_w);
    ctx.draw->AddRectFilled(ImVec2(thumb_x0, y + 2.0F),
                            ImVec2(std::max(thumb_x1, thumb_x0 + 6.0F), y + kScrollBarH - 2.0F),
                            ImGui::GetColorU32(ImGuiCol_ScrollbarGrab));

    if (!ImGui::IsItemActive()) return;
    const float delta = ImGui::GetIO().MouseDelta.x;
    if (delta == 0.0F) return;
    const double panned = view.scroll + (((double)delta / ctx.lane_w) * span);
    view.scroll = Editor::ClampScroll(panned, ctx.length, view.px_per_frame, ctx.lane_w);
}

void DrawHeaderSplitter(Ctx& ctx) {
    ImGui::SetCursorScreenPos(ImVec2(ctx.lane_x - 3.0F, ctx.lanes_top));
    ImGui::InvisibleButton("###tl_header_split",
                           ImVec2(6.0F, std::max(1.0F, ctx.lanes_bottom - ctx.lanes_top)));
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (!ImGui::IsItemActive()) return;
    Editor::View& view = ctx.editor->MutView();
    view.header_w = std::clamp(view.header_w + ImGui::GetIO().MouseDelta.x, Editor::kHeaderWidthMin,
                               Editor::kHeaderWidthMax);
}

float ContentHeight(const Ctx& ctx) {
    float total = OptionsBandHeight(*ctx.document);
    for (const Doc::Track& track : ctx.document->tracks)
        total += TrackHeight(track);
    return total;
}

void DrawTracks(Ctx& ctx) {
    ImGui::PushClipRect(ImVec2(ctx.header_x, ctx.lanes_top),
                        ImVec2(ctx.lane_x + ctx.lane_w, ctx.lanes_bottom), true);
    float y = ctx.lanes_top - ctx.editor->GetView().track_scroll;
    ctx.options_top = y;
    if (!ctx.document->options.empty()) {
        DrawOptionsBand(ctx, y);
        y += OptionsBandHeight(*ctx.document);
    }
    for (const Doc::Track& track : ctx.document->tracks) {
        const float height = TrackHeight(track);
        if (y > ctx.lanes_bottom) break;
        if (y + height >= ctx.lanes_top) {
            ctx.bands.push_back(TrackBand{.id = track.id, .y0 = y, .y1 = y + height});
            DrawTrackHeader(ctx, track, y, height);
            DrawTrackLane(ctx, track, y, height);
        }
        y += height;
    }
    DrawOptionsOverlay(ctx);
    ImGui::PopClipRect();
    ctx.draw->AddLine(ImVec2(ctx.lane_x, ctx.lanes_top), ImVec2(ctx.lane_x, ctx.lanes_bottom),
                      ImGui::GetColorU32(ImGuiCol_Border), 1.0F);
}

}

float FrameToX(const Ctx& ctx, double frame) {
    const Editor::View& view = ctx.editor->GetView();
    return ctx.lane_x + (float)((frame - view.scroll) * view.px_per_frame);
}

double XToFrame(const Ctx& ctx, float x) {
    const Editor::View& view = ctx.editor->GetView();
    if (view.px_per_frame <= 0.0) return 0.0;
    return view.scroll + ((double)(x - ctx.lane_x) / view.px_per_frame);
}

int CursorFrame(const Ctx& ctx) {
    const double frame = XToFrame(ctx, ImGui::GetIO().MousePos.x);
    return std::clamp((int)std::lround(frame), 0, std::max(0, ctx.length - 1));
}

void DashedVertical(const Ctx& ctx, float x, float y0, float y1, ImU32 color) {
    const auto steps = (int)std::floor((y1 - y0) / kDashPitch);
    for (int i = 0; i <= steps; i++) {
        const float from = y0 + ((float)i * kDashPitch);
        ctx.draw->AddLine(ImVec2(x, from), ImVec2(x, std::min(from + kDashLength, y1)), color,
                          1.0F);
    }
}

void PostSeek(int frame) {
    App::Global().PostCommand(PresetCmd::Wrap(PresetCmd::Seek{.frame = std::max(0, frame)}));
    App::Global().PostCommand(PresetCmd::Wrap(PresetCmd::SetPaused{.paused = true}));
}

void PostPaused(bool paused) {
    App::Global().PostCommand(PresetCmd::Wrap(PresetCmd::SetPaused{.paused = paused}));
}

void PublishDocument(const Ctx& ctx) {
    if (ctx.editor->Revision() == g_published) return;
    g_published = ctx.editor->Revision();
    App::Global().PostCommand(
        PresetCmd::Wrap(PresetCmd::ReplaceDocument{.document = ctx.editor->Snapshot()}));
}

void ApplyEdit(Ctx& ctx, const Editor::Edit& edit) {
    ctx.pending.push_back(edit);
}

float TrackHeight(const Doc::Track& track) {
    return RowHeight() + (kSubLaneH * (float)(Editor::LaneCount(track) - 1));
}

bool Active() {
    const Editor::State& editor = Editor::Global();
    if (!editor.Loaded()) return false;
    return App::Global().GetPresetStatus().id == editor.Document().id;
}

void RenderModals() {
    Editor::State& editor = Editor::Global();
    static unsigned owner = 0;
    if (editor.LoadId() != owner) {
        owner = editor.LoadId();
        ResetClipModal();
        ResetPalette();
        ResetDocumentModal();
        ResetOptionModal();
        ResetProblems();
        CloseCurveEditor();
    }
    const Editor::Request request = editor.TakeRequest();
    switch (request.kind) {
    case Editor::RequestKind::ClipProperties:
        if (request.index >= 0) editor.SelectKey(request.clip_id, request.index);
        RequestClipModal(request.clip_id, request.index >= 0);
        break;
    case Editor::RequestKind::CurveEditor:
        RequestCurveEditor(request.clip_id);
        break;
    case Editor::RequestKind::OptionProperties:
        RequestOptionModal(request.index);
        break;
    case Editor::RequestKind::AddCommand:
        RequestPalette(request.track_id, request.frame);
        break;
    case Editor::RequestKind::AddTrack:
        RequestTrackModal(request.track_id);
        break;
    case Editor::RequestKind::DocumentProperties:
        RequestDocumentModal();
        break;
    case Editor::RequestKind::None:
    default:
        break;
    }
    RenderPalette();
    RenderTrackModal();
    RenderClipModal();
    RenderDocumentModal();
    RenderOptionModal();
    RenderProblems();
}

void Render(float height) {
    Editor::State& editor = Editor::Global();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_PopupBg));
    ImGui::BeginChild("##timeline_editor", ImVec2(0, height), 1,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const Editor::DocPtr alive = editor.Snapshot();
    Ctx ctx;
    ctx.editor = &editor;
    ctx.document = alive.get();
    ctx.status = App::Global().GetPresetStatus();
    ctx.draw = ImGui::GetWindowDrawList();
    ctx.length = Editor::DocumentLength(*ctx.document);
    ctx.any_solo = std::ranges::any_of(ctx.document->tracks,
                                       [](const Doc::Track& track) { return track.solo; });

    DrawTransport(ctx);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float total_w = ImGui::GetContentRegionAvail().x;
    Editor::View& view = editor.MutView();
    view.header_w = std::clamp(view.header_w, Editor::kHeaderWidthMin, Editor::kHeaderWidthMax);
    ctx.header_x = origin.x;
    ctx.lane_x = origin.x + view.header_w;
    ctx.lane_w = std::max(1.0F, total_w - view.header_w);
    ctx.lanes_top = origin.y + kRulerH;
    ctx.lanes_bottom =
        std::max(ctx.lanes_top, origin.y + ImGui::GetContentRegionAvail().y - kScrollBarH - 4.0F);
    view.px_per_frame = Editor::ClampZoom(view.px_per_frame);
    view.scroll = Editor::ClampScroll(view.scroll, ctx.length, view.px_per_frame, ctx.lane_w);

    if (CurveEditorOpen(ctx)) {
        DrawCurveEditor(ctx);
    } else {
        DrawRuler(ctx);
        view.track_scroll = Editor::ClampTrackScroll(view.track_scroll, ContentHeight(ctx),
                                                     ctx.lanes_bottom - ctx.lanes_top);
        DrawTracks(ctx);
        DrawHeaderSplitter(ctx);
        UpdateDrag(ctx);
        UpdateBand(ctx);
        DrawPlayhead(ctx);
        DrawScrollBar(ctx);
        HandleShortcuts(ctx);
    }
    for (const Editor::Edit& edit : ctx.pending)
        editor.Apply(edit);
    PublishDocument(ctx);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

}
