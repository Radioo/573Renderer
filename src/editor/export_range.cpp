#include "editor/export_range.h"

#include <algorithm>

namespace Editor {

ExportRange RangeFromDrag(int anchor, int cursor, int length) {
    const int from = std::min(anchor, cursor);
    const int to = std::max(anchor, cursor);
    return ClampRange(ExportRange{.start = from, .end = to + 1, .active = true}, length);
}

ExportRange ClampRange(const ExportRange& range, int length) {
    if (!range.active || length <= 0) return ExportRange{};
    if (range.start >= length || range.end <= 0) return ExportRange{};
    ExportRange out = range;
    out.start = std::clamp(out.start, 0, length - 1);
    out.end = std::clamp(out.end, 1, length);
    if (out.end <= out.start) return ExportRange{};
    return out;
}

int RangeFrames(const ExportRange& range, int length) {
    if (!range.active) return length;
    return std::max(1, range.end - range.start);
}

bool FpsRatioAllowed(int document_fps, int export_fps) {
    if (document_fps <= 0 || export_fps <= 0) return false;
    if (export_fps >= document_fps) return export_fps % document_fps == 0;
    return document_fps % export_fps == 0;
}

}
