#pragma once

namespace Editor {

struct ExportRange {
    int start = 0;
    int end = 0;
    bool active = false;
};

ExportRange RangeFromDrag(int anchor, int cursor, int length);

ExportRange ClampRange(const ExportRange& range, int length);

int RangeFrames(const ExportRange& range, int length);

bool FpsRatioAllowed(int document_fps, int export_fps);

}
