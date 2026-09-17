#include "document/group_sprite.h"

#include "document/clip.h"
#include "document/image_shape.h"
#include "document/span_tags.h"
#include "document/tags.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr uint32_t kThreeD = 0x04000000;

struct Moved {
    std::size_t index = 0;
    uint32_t frame = 0;
};

std::string DepthText(uint16_t depth) {
    return "depth " + std::to_string(depth);
}

bool Overlaps(const Span& span, const GroupRange& range) {
    return span.first_frame <= range.last_frame && span.last_frame >= range.first_frame;
}

bool Grouped(uint16_t depth, const GroupRange& range) {
    return depth >= range.first_depth && depth <= range.last_depth;
}

std::string Unreachable(const AfpAnimation::Placement& placement) {
    if (placement.name) return "an instance name";
    if (placement.class_name) return "a class name";
    if (placement.clip_actions) return "a script";
    if ((placement.flags & kThreeD) != 0) return "a 3D placement";
    return {};
}

Support::Expected<void, std::string> CheckSpan(const AfpAnimation::Container& clip, uint16_t depth,
                                               const Span& span, const GroupRange& range) {
    const bool grouped = Grouped(depth, range);
    if (grouped && (span.first_frame < range.first_frame || span.last_frame > range.last_frame)) {
        return Support::Unexpected(
            DepthText(depth) + " runs from frame " + std::to_string(span.first_frame) + " to " +
            std::to_string(span.last_frame) + ", past the frames being grouped");
    }
    for (const std::size_t index : SpanTags(clip, depth, span)) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&clip.tags[index].body);
        if (placement == nullptr) continue;
        if (placement->clip_depth) {
            return Support::Unexpected(
                DepthText(depth) +
                " sets a clip depth on those frames, and how a clip depth reaches into a sprite "
                "is not known");
        }
        const std::string what = Unreachable(*placement);
        if (grouped && !what.empty()) {
            return Support::Unexpected(DepthText(depth) + " carries " + what +
                                       ", which would not reach it the same way inside a sprite");
        }
    }
    return {};
}

Support::Expected<void, std::string> CheckRange(const AfpAnimation::Container& clip,
                                                const GroupRange& range) {
    if (range.first_depth > range.last_depth)
        return Support::Unexpected(std::string("the depths to group run backwards"));
    if (range.first_frame > range.last_frame || range.last_frame >= clip.frames.size())
        return Support::Unexpected(std::string("the frames to group are not in the clip"));
    if (range.last_frame + 1 > std::numeric_limits<uint16_t>::max())
        return Support::Unexpected(std::string("a placement end frame is a u16"));
    bool any = false;
    for (const DepthRow& row : DepthRows(clip)) {
        for (const Span& span : row.spans) {
            if (!Overlaps(span, range)) continue;
            auto checked = CheckSpan(clip, row.depth, span, range);
            if (!checked) return Support::Unexpected(checked.error());
            any = any || Grouped(row.depth, range);
        }
    }
    if (!any) {
        return Support::Unexpected("depths " + std::to_string(range.first_depth) + " to " +
                                   std::to_string(range.last_depth) +
                                   " hold nothing on those frames");
    }
    return {};
}

std::vector<Moved> MovedTags(const AfpAnimation::Container& clip, const GroupRange& range) {
    std::vector<Moved> moved;
    const auto end = static_cast<uint32_t>(clip.frames.size());
    for (uint32_t frame = range.first_frame; frame <= range.last_frame + 1 && frame < end;
         frame++) {
        const AfpAnimation::Frame& owner = clip.frames[frame];
        for (uint32_t i = 0; i < owner.tag_count; i++) {
            const std::size_t index = owner.first_tag + i;
            const AfpAnimation::Tag& tag = clip.tags[index];
            const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
            const auto* remove = std::get_if<AfpAnimation::Remove>(&tag.body);
            const bool placed = placement != nullptr && Grouped(placement->depth, range) &&
                                frame <= range.last_frame;
            const bool removed =
                remove != nullptr && Grouped(remove->depth, range) && frame > range.first_frame;
            if (placed || removed) moved.push_back(Moved{.index = index, .frame = frame});
        }
    }
    return moved;
}

AfpAnimation::Container Nested(const AfpAnimation::Container& clip, const std::vector<Moved>& moved,
                               const GroupRange& range) {
    AfpAnimation::Container nested;
    const uint32_t frames = range.last_frame - range.first_frame + 1;
    nested.frames.assign(frames, AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    for (uint32_t frame = 0; frame < frames; frame++) {
        nested.frames[frame].first_tag = static_cast<uint32_t>(nested.tags.size());
        for (const Moved& one : moved) {
            if (one.frame != range.first_frame + frame) continue;
            AfpAnimation::Tag tag = clip.tags[one.index];
            auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
            if (placement != nullptr && placement->end_frame >= range.first_frame) {
                placement->end_frame =
                    static_cast<uint16_t>(placement->end_frame - range.first_frame);
            }
            nested.tags.push_back(std::move(tag));
            nested.frames[frame].tag_count++;
        }
    }
    return nested;
}

}

Support::Expected<uint16_t, std::string> GroupIntoSprite(AfpAnimation::Animation& animation,
                                                         const GroupRange& range) {
    AfpAnimation::Animation edited = animation;
    auto found = RequireClip(edited, range.clip);
    if (!found) return Support::Unexpected(found.error());
    AfpAnimation::Container& clip = **found;
    auto checked = CheckRange(clip, range);
    if (!checked) return Support::Unexpected(checked.error());
    const auto id = NextCharacterId(edited);
    if (!id) return Support::Unexpected(id.error());
    if (edited.root.frames.empty())
        return Support::Unexpected(std::string("the animation has no frame to define a sprite in"));

    const std::vector<Moved> moved = MovedTags(clip, range);
    AfpAnimation::Sprite sprite{.id = *id, .container = Nested(clip, moved, range)};
    for (std::size_t i = moved.size(); i > 0; i--)
        EraseTag(clip, moved[i - 1].index);

    AfpAnimation::Placement placement;
    placement.depth = range.first_depth;
    placement.end_frame = static_cast<uint16_t>(range.last_frame + 1);
    placement.character = *id;
    InsertTag(clip, range.first_frame, AfpAnimation::Tag{std::move(placement)});
    if (range.last_frame + 1 < clip.frames.size()) {
        InsertTagFirst(
            clip, range.last_frame + 1,
            AfpAnimation::Tag{AfpAnimation::Remove{.unread_word = 0, .depth = range.first_depth}});
    }
    InsertTag(edited.root, 0, AfpAnimation::Tag{std::move(sprite)});
    animation = std::move(edited);
    return *id;
}

}
