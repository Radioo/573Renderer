#include "document/span_sequence.h"

#include "document/clip.h"
#include "document/span_edit.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Document {

Support::Expected<std::vector<SpanShift>, std::string>
SequenceSpans(AfpAnimation::Animation& animation, ClipId clip, std::vector<uint16_t> depths,
              uint32_t frame) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    std::ranges::sort(depths);
    if (depths.size() < 2)
        return Support::Unexpected(std::string("sequencing needs at least two depths"));

    AfpAnimation::Animation edited = animation;
    std::vector<SpanShift> shifts;
    std::optional<uint32_t> previous_last;
    for (const uint16_t depth : depths) {
        const std::optional<Span> span = SpanOfDepth(**found, depth, frame);
        if (!span) {
            return Support::Unexpected("depth " + std::to_string(depth) +
                                       " holds nothing on frame " + std::to_string(frame));
        }
        if (!previous_last) {
            previous_last = span->last_frame;
            continue;
        }
        const int64_t by = static_cast<int64_t>(*previous_last) + 1 - span->first_frame;
        auto moved = MoveSpan(edited, clip, depth, span->first_frame, by);
        if (!moved) return Support::Unexpected(moved.error());
        previous_last = moved->last_frame;
        shifts.push_back(SpanShift{.depth = depth, .frame = span->first_frame, .by = by});
    }
    animation = std::move(edited);
    return shifts;
}

}
