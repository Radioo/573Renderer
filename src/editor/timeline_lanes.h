#pragma once

#include "preset/doc/preset_document.h"

namespace Editor {

struct LaneMetrics {
    float row = 0.0F;
    float clip = 0.0F;
    float sub_lane = 0.0F;
    float sub_clip = 0.0F;
};

LaneMetrics LaneMetricsFor(float text_height, float pad_y);

float LaneLabelY(float bar_y0, float bar_height, float text_height, bool keyed);

int LaneCount(const Preset::Doc::Track& track);

int LaneOf(const Preset::Doc::Track& track, const Preset::Doc::Clip& clip);

}
