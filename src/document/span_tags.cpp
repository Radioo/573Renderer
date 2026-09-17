#include "document/span_tags.h"

#include "document/placement_edit.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

bool IsRemoveOf(const AfpAnimation::Tag& tag, uint16_t depth) {
    const auto* remove = std::get_if<AfpAnimation::Remove>(&tag.body);
    return remove != nullptr && remove->depth == depth;
}

bool IsPlacementOf(const AfpAnimation::Tag& tag, uint16_t depth) {
    const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
    return placement != nullptr && placement->depth == depth;
}

std::vector<std::size_t> SpanTags(const AfpAnimation::Container& clip, uint16_t depth,
                                  const Span& span) {
    std::vector<std::size_t> found;
    const uint32_t closing = span.last_frame + 1;
    for (uint32_t frame = span.first_frame; frame <= closing && frame < clip.frames.size();
         frame++) {
        const AfpAnimation::Frame& owner = clip.frames[frame];
        for (uint32_t i = 0; i < owner.tag_count; i++) {
            const std::size_t index = owner.first_tag + i;
            if (index >= clip.tags.size()) break;
            const AfpAnimation::Tag& tag = clip.tags[index];
            const bool ours = frame == closing ? IsRemoveOf(tag, depth) : IsPlacementOf(tag, depth);
            if (ours) found.push_back(index);
        }
    }
    return found;
}

void InsertTagFirst(AfpAnimation::Container& clip, uint32_t frame, AfpAnimation::Tag tag) {
    AfpAnimation::Frame& owner = clip.frames[frame];
    clip.tags.insert(clip.tags.begin() + static_cast<std::ptrdiff_t>(owner.first_tag),
                     std::move(tag));
    owner.tag_count++;
    for (std::size_t i = frame + 1; i < clip.frames.size(); i++)
        clip.frames[i].first_tag++;
}

bool TouchedBetween(const AfpAnimation::Container& clip, uint16_t depth, uint32_t first,
                    uint32_t last) {
    for (uint32_t frame = first; frame <= last && frame < clip.frames.size(); frame++) {
        const AfpAnimation::Frame& owner = clip.frames[frame];
        for (uint32_t i = 0; i < owner.tag_count; i++) {
            const std::size_t index = owner.first_tag + i;
            if (index >= clip.tags.size()) break;
            if (IsPlacementOf(clip.tags[index], depth) || IsRemoveOf(clip.tags[index], depth))
                return true;
        }
    }
    return false;
}

Support::Expected<void, std::string> CheckFree(const AfpAnimation::Container& clip, uint16_t depth,
                                               uint32_t first, uint32_t last) {
    for (uint32_t frame = first; frame <= last; frame++) {
        if (LivePlacementTag(clip, depth, frame)) {
            return Support::Unexpected("depth " + std::to_string(depth) +
                                       " already shows something on frame " +
                                       std::to_string(frame));
        }
    }
    return {};
}

}
