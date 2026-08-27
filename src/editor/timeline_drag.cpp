#include "editor/timeline_drag.h"

#include "editor/timeline_edits.h"
#include "editor/timeline_view.h"
#include "preset/doc/preset_document.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Editor {

namespace Doc = Preset::Doc;

namespace {

bool Nearest(const std::vector<int>& candidates, int frame, int tolerance, int& out) {
    bool found = false;
    int best = 0;
    for (const int candidate : candidates) {
        const int distance = std::abs(candidate - frame);
        if (distance > tolerance) continue;
        if (found && distance >= std::abs(best - frame)) continue;
        best = candidate;
        found = true;
    }
    if (found) out = best;
    return found;
}

void CollectEdges(const Doc::Document& document, std::string_view ignore_clip_id, int length,
                  std::vector<int>& edges, std::vector<int>& keys) {
    for (const Doc::Track& track : document.tracks) {
        for (const Doc::Clip& clip : track.clips) {
            if (clip.id == ignore_clip_id) continue;
            edges.push_back(clip.start);
            edges.push_back(ClipEnd(clip, length));
            for (const Doc::Key& key : clip.keys)
                keys.push_back(clip.start + key.at);
        }
    }
}

std::vector<int> TickFrames(int length, double px_per_frame) {
    std::vector<int> ticks;
    const int step = RulerStep(px_per_frame);
    for (int frame = 0; frame <= length; frame += step)
        ticks.push_back(frame);
    return ticks;
}

const Doc::Clip* ClipOf(const Doc::Document& document, std::string_view clip_id) {
    const ClipRef ref = FindClip(document, clip_id);
    if (!ref.Valid()) return nullptr;
    return &document.tracks[(std::size_t)ref.track].clips[(std::size_t)ref.clip];
}

std::string DestinationTrack(const Doc::Document& document, const DragStart& drag,
                             const DragInput& input) {
    const int home = FindTrack(document, drag.track_id);
    const int wanted = FindTrack(document, input.track_id);
    if (home < 0 || wanted < 0) return drag.track_id;
    if (document.tracks[(std::size_t)wanted].kind != document.tracks[(std::size_t)home].kind)
        return drag.track_id;
    if (document.tracks[(std::size_t)wanted].locked) return drag.track_id;
    return input.track_id;
}

int ClampRight(const std::vector<Span>& peers, int start, int end) {
    int limit = end;
    for (const Span& peer : peers) {
        if (peer.start < start) continue;
        limit = std::min(limit, peer.start);
    }
    return std::max(start + 1, limit);
}

int ClampLeft(const std::vector<Span>& peers, int start, int end) {
    int limit = start;
    for (const Span& peer : peers) {
        if (peer.end > end) continue;
        limit = std::max(limit, peer.end);
    }
    return std::min(limit, end - 1);
}

DragResult ResolveMove(const Doc::Document& document, const DragStart& drag,
                       const DragInput& input) {
    DragResult result;
    result.track_id = DestinationTrack(document, drag, input);
    const int span = drag.end.has_value() ? (*drag.end - drag.start) : 0;
    const int raw = std::max(0, drag.start + (input.cursor_frame - drag.grab_frame));

    const Snap head = SnapFrame(document, raw, input, drag.clip_id);
    int start = head.frame;
    result.snapped = head.snapped;
    result.snap_frame = head.frame;
    if (!head.snapped && span > 0) {
        const Snap tail = SnapFrame(document, raw + span, input, drag.clip_id);
        if (tail.snapped) {
            start = std::max(0, tail.frame - span);
            result.snapped = true;
            result.snap_frame = tail.frame;
        }
    }

    result.start = start;
    if (drag.end.has_value()) result.end = start + span;
    result.allowed = !Overlaps(document, result.track_id, drag.clip_id, result.start, result.end);
    return result;
}

DragResult ResolveResize(const Doc::Document& document, const DragStart& drag,
                         const DragInput& input) {
    DragResult result;
    result.track_id = drag.track_id;
    result.start = drag.start;
    result.end = drag.end;

    const int length = DocumentLength(document);
    const std::vector<Span> peers = PrimaryPeers(document, drag.track_id, drag.clip_id);
    const Snap edge = SnapFrame(document, input.cursor_frame, input, drag.clip_id);
    result.snapped = edge.snapped;
    result.snap_frame = edge.frame;

    if (drag.mode == DragMode::ResizeRight) {
        if (edge.frame >= length) {
            result.end.reset();
            return result;
        }
        result.end = ClampRight(peers, drag.start, std::max(drag.start + 1, edge.frame));
        return result;
    }

    const int finish = drag.end.value_or(length);
    result.start = ClampLeft(peers, std::max(0, edge.frame), finish);
    if (drag.end.has_value()) result.end = finish;
    return result;
}

}

Zone ZoneAt(float clip_x0, float clip_x1, float x) {
    if (x < clip_x0 || x > clip_x1) return Zone::None;
    const float handle = std::min(kClipHandlePx, (clip_x1 - clip_x0) * 0.3F);
    if (x < clip_x0 + handle) return Zone::LeftHandle;
    if (x > clip_x1 - handle) return Zone::RightHandle;
    return Zone::Body;
}

Snap SnapFrame(const Doc::Document& document, int frame, const DragInput& input,
               std::string_view ignore_clip_id) {
    Snap result;
    result.frame = frame;
    if (!input.snap || input.px_per_frame <= 0.0) return result;

    const int length = DocumentLength(document);
    const auto tolerance = (int)std::lround(kSnapPx / input.px_per_frame);

    std::vector<int> playhead;
    if (input.playhead >= 0) playhead.push_back(input.playhead);
    std::vector<int> edges;
    std::vector<int> keys;
    CollectEdges(document, ignore_clip_id, length, edges, keys);
    const std::vector<int> bounds{0, length};
    const std::vector<int> ticks = TickFrames(length, input.px_per_frame);

    const std::array<const std::vector<int>*, 5> groups = {&playhead, &edges, &keys, &bounds,
                                                           &ticks};
    for (const std::vector<int>* group : groups) {
        int snapped = 0;
        if (!Nearest(*group, frame, tolerance, snapped)) continue;
        result.frame = snapped;
        result.snapped = true;
        return result;
    }
    return result;
}

DragResult ResolveDrag(const Doc::Document& document, const DragStart& drag,
                       const DragInput& input) {
    DragResult result;
    result.track_id = drag.track_id;
    result.start = drag.start;
    result.end = drag.end;
    const Doc::Clip* clip = ClipOf(document, drag.clip_id);
    if (clip == nullptr || drag.mode == DragMode::None) return result;
    if (drag.mode == DragMode::Move) return ResolveMove(document, drag, input);
    if (IsEvent(*clip)) return result;
    return ResolveResize(document, drag, input);
}

}
