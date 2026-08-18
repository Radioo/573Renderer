#pragma once

#include "editor/preset_editor_state.h"
#include "editor/timeline_drag.h"
#include "editor/timeline_lanes.h"
#include "imgui.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "state/telemetry.h"

#include <string>
#include <vector>

namespace Panels::Timeline {

struct TrackBand {
    std::string id = {};
    float y0 = 0.0F;
    float y1 = 0.0F;
};

struct ClipRect {
    std::string id = {};
    float x0 = 0.0F;
    float x1 = 0.0F;
    float y0 = 0.0F;
    float y1 = 0.0F;
};

inline constexpr float kRulerH = 34.0F;
inline constexpr float kScrollBarH = 12.0F;
inline constexpr float kDashPitch = 8.0F;
inline constexpr float kDashLength = 4.0F;

struct Ctx {
    Editor::State* editor = nullptr;
    const Preset::Doc::Document* document = nullptr;
    App::PresetStatus status = {};
    ImDrawList* draw = nullptr;
    float lane_x = 0.0F;
    float lane_w = 0.0F;
    float header_x = 0.0F;
    float lanes_top = 0.0F;
    float lanes_bottom = 0.0F;
    int length = 1;
    float options_top = 0.0F;
    bool any_solo = false;
    std::vector<Editor::Edit> pending = {};
    std::vector<TrackBand> bands = {};
    std::vector<ClipRect> clip_rects = {};
};

float FrameToX(const Ctx& ctx, double frame);

double XToFrame(const Ctx& ctx, float x);

int CursorFrame(const Ctx& ctx);

void DashedVertical(const Ctx& ctx, float x, float y0, float y1, ImU32 color);

void PostSeek(int frame);

void PostPaused(bool paused);

void PublishDocument(const Ctx& ctx);

void ApplyEdit(Ctx& ctx, const Editor::Edit& edit);

ImU32 CommandColor(Preset::Doc::CommandType type);

ImU32 TrackKindColor(Preset::Doc::TrackKind kind);

const char* KindBadge(Preset::Doc::TrackKind kind);

std::string Ellipsized(const std::string& text, float width);

float ToggleSide();

Editor::LaneMetrics LaneSizes();

float RowHeight();

float TrackHeight(const Preset::Doc::Track& track);

void DrawTransport(Ctx& ctx);

void DrawRuler(Ctx& ctx);

void DrawPlayhead(const Ctx& ctx);

void DrawTrackHeader(Ctx& ctx, const Preset::Doc::Track& track, float y, float height);

void DrawTrackLane(Ctx& ctx, const Preset::Doc::Track& track, float y, float height);

void DrawClip(Ctx& ctx, const Preset::Doc::Track& track, const Preset::Doc::Clip& clip, float y);

void BeginBand(float x, float y);

void UpdateBand(Ctx& ctx);

void BeginDrag(Ctx& ctx, const Preset::Doc::Track& track, const Preset::Doc::Clip& clip,
               Editor::DragMode mode);

void UpdateDrag(Ctx& ctx);

bool DragActive();

void CancelDrag();

void ClipMenuItems(Ctx& ctx, const Preset::Doc::Clip& clip);

void AddKeyHere(Ctx& ctx, const std::string& clip_id);

void AddTransitionBetween(Ctx& ctx, const std::string& a_id, const std::string& b_id);

float OptionsBandHeight(const Preset::Doc::Document& document);

void DrawOptionsBand(Ctx& ctx, float y);

void DrawOptionsOverlay(const Ctx& ctx);

void RequestCurveEditor(std::string clip_id);

void CloseCurveEditor();

bool CurveEditorOpen(const Ctx& ctx);

void DrawCurveEditor(Ctx& ctx);

void HandleShortcuts(Ctx& ctx);

void DeleteSelection(Ctx& ctx);

void CopySelection(Ctx& ctx, bool cut);

void PasteAt(Ctx& ctx, int frame);

}
