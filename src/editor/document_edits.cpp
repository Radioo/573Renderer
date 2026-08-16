#include "editor/document_edits.h"

#include "preset/doc/preset_document.h"

#include <algorithm>
#include <cmath>

namespace Editor {

namespace Doc = Preset::Doc;

namespace {

double Ratio(const Doc::Document& document, int target_fps) {
    const int from = std::max(1, document.fps);
    return (double)target_fps / (double)from;
}

int Scale(int frame, double ratio, Rounding rounding) {
    const double scaled = (double)frame * ratio;
    switch (rounding) {
    case Rounding::Floor:
        return (int)std::floor(scaled);
    case Rounding::Ceil:
        return (int)std::ceil(scaled);
    case Rounding::Nearest:
    default:
        return (int)std::lround(scaled);
    }
}

bool Fractional(int frame, double ratio) {
    const double scaled = (double)frame * ratio;
    return std::abs(scaled - std::round(scaled)) > 1.0e-9;
}

}

FpsPreview PreviewConvertFps(const Doc::Document& document, int target_fps) {
    FpsPreview preview;
    if (target_fps <= 0 || target_fps == document.fps) return preview;
    const double ratio = Ratio(document, target_fps);
    for (const Doc::Track& track : document.tracks) {
        for (const Doc::Clip& clip : track.clips) {
            if (Fractional(clip.start, ratio)) preview.edges++;
            if (clip.end.has_value() && Fractional(*clip.end, ratio)) preview.edges++;
            for (const Doc::Key& key : clip.keys) {
                if (Fractional(key.at, ratio)) preview.keys++;
            }
        }
    }
    for (const Doc::Marker& marker : document.markers) {
        if (Fractional(marker.frame, ratio)) preview.edges++;
    }
    return preview;
}

bool ConvertFps(Doc::Document& document, int target_fps, Rounding rounding) {
    if (target_fps <= 0 || target_fps == document.fps) return false;
    const double ratio = Ratio(document, target_fps);
    for (Doc::Track& track : document.tracks) {
        for (Doc::Clip& clip : track.clips) {
            const int start = Scale(clip.start, ratio, rounding);
            if (clip.end.has_value()) {
                clip.end = std::max(start + 1, Scale(*clip.end, ratio, rounding));
            }
            clip.start = start;
            for (Doc::Key& key : clip.keys)
                key.at = Scale(key.at, ratio, rounding);
        }
    }
    for (Doc::Marker& marker : document.markers)
        marker.frame = Scale(marker.frame, ratio, rounding);
    std::ranges::stable_sort(document.markers, [](const Doc::Marker& a, const Doc::Marker& b) {
        return a.frame < b.frame;
    });
    if (document.length.has_value()) {
        document.length = std::max(1, Scale(*document.length, ratio, rounding));
    }
    document.fps = target_fps;
    return true;
}

}
