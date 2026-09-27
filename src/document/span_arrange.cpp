#include "document/span_arrange.h"

#include "document/clip.h"
#include "document/span_edit.h"
#include "document/span_tags.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

std::vector<uint16_t> ShownAt(const AfpAnimation::Container& clip, uint32_t frame) {
    std::vector<uint16_t> shown;
    for (const DepthRow& row : DepthRows(clip)) {
        const bool open = std::ranges::any_of(row.spans, [frame](const Span& span) {
            return frame >= span.first_frame && frame <= span.last_frame;
        });
        if (open) shown.push_back(row.depth);
    }
    return shown;
}

std::vector<uint16_t> Passed(const std::vector<uint16_t>& shown, uint16_t depth, Arrange how) {
    const auto at = std::ranges::find(shown, depth);
    std::vector<uint16_t> passed;
    if (how == Arrange::Forward || how == Arrange::Front) {
        passed.assign(std::next(at), shown.end());
    } else {
        passed.assign(shown.begin(), at);
        std::ranges::reverse(passed);
    }
    if ((how == Arrange::Forward || how == Arrange::Backward) && passed.size() > 1)
        passed.resize(1);
    return passed;
}

Support::Expected<void, std::string> CheckUnmasked(const AfpAnimation::Container& clip,
                                                   uint16_t depth, uint32_t frame) {
    const std::optional<Span> span = SpanOfDepth(clip, depth, frame);
    if (!span) return {};
    for (const std::size_t index : SpanTags(clip, depth, *span)) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&clip.tags[index].body);
        if (placement == nullptr || !placement->clip_depth) continue;
        return Support::Unexpected("depth " + std::to_string(depth) + " masks the depths up to " +
                                   std::to_string(*placement->clip_depth) +
                                   ", which would change if it moved");
    }
    return {};
}

Support::Expected<void, std::string> Swap(AfpAnimation::Animation& animation, ClipId clip,
                                          uint32_t frame, std::pair<uint16_t, uint16_t> depths,
                                          uint16_t scratch) {
    const auto [depth, other] = depths;
    const std::array<std::pair<uint16_t, uint16_t>, 3> steps{
        {{depth, scratch}, {other, depth}, {scratch, other}}};
    for (const auto& [from, to] : steps) {
        auto changed = ChangeSpanDepth(animation, clip, from, frame, to);
        if (!changed) return Support::Unexpected(changed.error());
    }
    return {};
}

std::string AlreadyThere(uint16_t depth, uint32_t frame, Arrange how) {
    const bool up = how == Arrange::Forward || how == Arrange::Front;
    return "depth " + std::to_string(depth) + " is already at the " + (up ? "front" : "back") +
           " on frame " + std::to_string(frame);
}

}

Support::Expected<std::vector<DepthChange>, std::string>
ArrangeSpan(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth, uint32_t frame,
            Arrange how) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    const AfpAnimation::Container& target = **found;
    const std::vector<uint16_t> shown = ShownAt(target, frame);
    if (std::ranges::find(shown, depth) == shown.end()) {
        return Support::Unexpected("depth " + std::to_string(depth) + " holds nothing on frame " +
                                   std::to_string(frame));
    }
    const std::vector<uint16_t> passed = Passed(shown, depth, how);
    if (passed.empty()) return Support::Unexpected(AlreadyThere(depth, frame, how));
    auto unmasked = CheckUnmasked(target, depth, frame);
    for (const uint16_t other : passed) {
        if (!unmasked) break;
        unmasked = CheckUnmasked(target, other, frame);
    }
    if (!unmasked) return Support::Unexpected(unmasked.error());
    const std::optional<uint16_t> scratch = UnusedDepth(target);
    if (!scratch) return Support::Unexpected(std::string("every depth of the clip is in use"));

    AfpAnimation::Animation edited = animation;
    std::vector<DepthChange> changes{{.from = depth, .to = passed.back()}};
    uint16_t at = depth;
    for (const uint16_t other : passed) {
        auto swapped = Swap(edited, clip, frame, {at, other}, *scratch);
        if (!swapped) return Support::Unexpected(swapped.error());
        changes.push_back(DepthChange{.from = other, .to = at});
        at = other;
    }
    animation = std::move(edited);
    return changes;
}

}
