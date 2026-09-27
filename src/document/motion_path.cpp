#include "document/motion_path.h"

#include "document/keyframes.h"
#include "document/placement_effect.h"
#include "document/span_edit.h"
#include "document/span_tags.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr double kPixelsPerUnit = 0.05;
constexpr uint32_t kThreeD = 0x04000000;
constexpr std::size_t kMoveX = 4;
constexpr std::size_t kMoveY = 5;
constexpr std::string_view kTranslation = "Translation";

bool AnyThreeD(const AfpAnimation::Container& clip, uint16_t depth, const Span& span) {
    return std::ranges::any_of(SpanTags(clip, depth, span), [&clip](std::size_t index) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&clip.tags[index].body);
        return placement != nullptr && (placement->flags & kThreeD) != 0;
    });
}

std::vector<uint32_t> KeyedFrames(const std::vector<Track>& tracks) {
    std::vector<uint32_t> frames;
    const auto track = std::ranges::find(tracks, kTranslation, &Track::property);
    if (track == tracks.end()) return frames;
    for (const Keyframe& key : track->keys)
        frames.push_back(key.frame);
    return frames;
}

}

std::vector<PathPoint> MotionPath(const AfpAnimation::Container& clip, uint16_t depth,
                                  uint32_t frame, const std::vector<Track>& tracks) {
    std::vector<PathPoint> path;
    const std::optional<Span> span = SpanOfDepth(clip, depth, frame);
    if (!span || AnyThreeD(clip, depth, *span)) return path;
    const std::vector<uint32_t> keyed = KeyedFrames(tracks);
    for (const auto& [at, state] : ReplayDepth(clip, depth, span->first_frame, span->last_frame)) {
        path.push_back(PathPoint{
            .frame = at,
            .at = {state.matrix[kMoveX] * kPixelsPerUnit, state.matrix[kMoveY] * kPixelsPerUnit},
            .keyed = std::ranges::find(keyed, at) != keyed.end()});
    }
    return path;
}

}
