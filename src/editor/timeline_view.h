#pragma once

#include "editor/export_range.h"

namespace Editor {

inline constexpr double kZoomMin = 0.05;
inline constexpr double kZoomMax = 8.0;
inline constexpr double kZoomDefault = 0.5;
inline constexpr float kLabelGapPx = 56.0F;
inline constexpr float kEditorHeightDefault = 280.0F;
inline constexpr float kEditorHeightMin = 120.0F;
inline constexpr float kHeaderWidthDefault = 200.0F;
inline constexpr float kHeaderWidthMin = 120.0F;
inline constexpr float kHeaderWidthMax = 320.0F;

struct View {
    ExportRange export_range = {};
    double px_per_frame = kZoomDefault;
    double scroll = 0.0;
    float track_scroll = 0.0F;
    float header_w = kHeaderWidthDefault;
    float height = kEditorHeightDefault;
    bool snap = true;
};

int RulerStep(double px_per_frame);

double ClampZoom(double px_per_frame);

double FitZoom(int length, float width);

double ClampScroll(double scroll, int length, double px_per_frame, float width);

float ClampTrackScroll(float scroll, float content_h, float visible_h);

}
