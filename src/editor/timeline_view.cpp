#include "editor/timeline_view.h"

#include <algorithm>
#include <array>

namespace Editor {

namespace {

constexpr std::array<int, 8> kSteps = {1, 5, 10, 30, 60, 300, 600, 1800};

}

int RulerStep(double px_per_frame, float label_gap) {
    for (const int step : kSteps) {
        if ((double)step * px_per_frame >= (double)label_gap) return step;
    }
    return kSteps.back();
}

double ClampZoom(double px_per_frame) {
    return std::clamp(px_per_frame, kZoomMin, kZoomMax);
}

double FitZoom(int length, float width) {
    if (length <= 0 || width <= 0.0F) return kZoomDefault;
    return ClampZoom((double)width / (double)length);
}

float ClampTrackScroll(float scroll, float content_h, float visible_h) {
    return std::clamp(scroll, 0.0F, std::max(0.0F, content_h - visible_h));
}

double ClampScroll(double scroll, int length, double px_per_frame, float width) {
    if (px_per_frame <= 0.0) return 0.0;
    const double visible = (double)width / px_per_frame;
    const double last = std::max(0.0, (double)length - visible);
    return std::clamp(scroll, 0.0, last);
}

}
