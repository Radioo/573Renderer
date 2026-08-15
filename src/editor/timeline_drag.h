#pragma once

#include "preset/doc/preset_document.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Editor {

inline constexpr float kClipHandlePx = 6.0F;
inline constexpr double kSnapPx = 8.0;

enum class Zone : uint8_t {
    None,
    Body,
    LeftHandle,
    RightHandle,
};

Zone ZoneAt(float clip_x0, float clip_x1, float x);

enum class DragMode : uint8_t {
    None,
    Move,
    ResizeLeft,
    ResizeRight,
};

struct DragStart {
    DragMode mode = DragMode::None;
    std::string clip_id = {};
    std::string track_id = {};
    int start = 0;
    std::optional<int> end = {};
    int grab_frame = 0;
    bool duplicate = false;
};

struct DragInput {
    int cursor_frame = 0;
    std::string track_id = {};
    int playhead = -1;
    double px_per_frame = 1.0;
    bool snap = true;
};

struct Snap {
    int frame = 0;
    bool snapped = false;
};

struct DragResult {
    int start = 0;
    std::optional<int> end = {};
    std::string track_id = {};
    int snap_frame = 0;
    bool snapped = false;
    bool allowed = true;
};

Snap SnapFrame(const Preset::Doc::Document& document, int frame, const DragInput& input,
               std::string_view ignore_clip_id);

DragResult ResolveDrag(const Preset::Doc::Document& document, const DragStart& drag,
                       const DragInput& input);

}
