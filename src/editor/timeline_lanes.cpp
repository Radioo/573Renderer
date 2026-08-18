#include "editor/timeline_lanes.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"

#include <algorithm>
#include <vector>

namespace Editor {

namespace Doc = Preset::Doc;

namespace {

constexpr float kRowMinH = 26.0F;
constexpr float kRowPad = 4.0F;
constexpr float kClipMinH = 22.0F;
constexpr float kSubClipMinH = 14.0F;
constexpr float kSubLaneGap = 2.0F;
constexpr float kKeyBandH = 6.0F;

bool IsModifier(const Doc::Clip& clip) {
    return Doc::TraitsFor(Doc::TypeOf(clip.command)).family == Doc::Family::None;
}

bool HasPrimary(const Doc::Track& track) {
    return std::ranges::any_of(track.clips,
                               [](const Doc::Clip& clip) { return !IsModifier(clip); });
}

std::vector<Doc::CommandType> ModifierTypes(const Doc::Track& track) {
    std::vector<Doc::CommandType> types;
    for (const Doc::Clip& clip : track.clips) {
        if (!IsModifier(clip)) continue;
        const Doc::CommandType type = Doc::TypeOf(clip.command);
        if (std::ranges::find(types, type) != types.end()) continue;
        types.push_back(type);
    }
    return types;
}

}

LaneMetrics LaneMetricsFor(float text_height, float pad_y) {
    const float label = text_height + (2.0F * pad_y);
    LaneMetrics metrics;
    metrics.clip = std::max(kClipMinH, label);
    metrics.sub_clip = std::max(kSubClipMinH, label);
    metrics.sub_lane = metrics.sub_clip + kSubLaneGap;
    metrics.row = std::max(kRowMinH, metrics.clip + kRowPad);
    return metrics;
}

float LaneLabelY(float bar_y0, float bar_height, float text_height, bool keyed) {
    const float room = std::max(0.0F, bar_height - text_height);
    return bar_y0 + (keyed ? std::min(kKeyBandH, room) : room * 0.5F);
}

int LaneCount(const Doc::Track& track) {
    if (!HasPrimary(track)) return 1;
    return 1 + (int)ModifierTypes(track).size();
}

int LaneOf(const Doc::Track& track, const Doc::Clip& clip) {
    if (!IsModifier(clip) || !HasPrimary(track)) return 0;
    const std::vector<Doc::CommandType> types = ModifierTypes(track);
    const auto it = std::ranges::find(types, Doc::TypeOf(clip.command));
    if (it == types.end()) return 1;
    return 1 + (int)(it - types.begin());
}

}
