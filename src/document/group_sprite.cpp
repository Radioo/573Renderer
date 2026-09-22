#include "document/group_sprite.h"

#include "document/clip.h"
#include "document/image_shape.h"
#include "document/span_edit.h"
#include "document/span_tags.h"
#include "document/tags.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ranges>
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

const AfpAnimation::Container* SpriteDefinition(const AfpAnimation::Container& root, uint16_t id) {
    for (const AfpAnimation::Tag& tag : root.tags) {
        const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body);
        if (sprite != nullptr && sprite->id == id) return &sprite->container;
    }
    return nullptr;
}

std::size_t Uses(const AfpAnimation::Container& clip, uint16_t id) {
    std::size_t uses = 0;
    for (const AfpAnimation::Tag& tag : clip.tags) {
        if (const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body)) {
            if (placement->character == id) uses++;
        } else if (const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body)) {
            uses += Uses(sprite->container, id);
        }
    }
    return uses;
}

bool Bare(const AfpAnimation::Placement& placement) {
    AfpAnimation::Placement bare = placement;
    bare.depth = 0;
    bare.end_frame = 0;
    bare.character.reset();
    return bare == AfpAnimation::Placement{};
}

Support::Expected<void, std::string> CheckContents(const AfpAnimation::Container& inside) {
    if (!inside.labels.empty() || (inside.script_labels && !inside.script_labels->empty())) {
        return Support::Unexpected(
            std::string("the sprite has labels, which would name nothing once it is gone"));
    }
    for (const AfpAnimation::Tag& tag : inside.tags) {
        if (std::holds_alternative<AfpAnimation::Remove>(tag.body)) continue;
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement == nullptr) {
            return Support::Unexpected(
                std::string("the sprite holds more than placements and removes"));
        }
        const std::string what =
            placement->clip_depth ? std::string("a clip depth") : Unreachable(*placement);
        if (!what.empty()) {
            return Support::Unexpected("the sprite places " + DepthText(placement->depth) +
                                       " with " + what +
                                       ", which would not reach it the same way outside it");
        }
    }
    return {};
}

Support::Expected<void, std::string> CheckStacking(const AfpAnimation::Container& clip,
                                                   uint16_t depth, const Span& span,
                                                   const std::vector<DepthRow>& children) {
    uint16_t low = depth;
    uint16_t high = depth;
    for (const DepthRow& child : children) {
        low = std::min(low, child.depth);
        high = std::max(high, child.depth);
    }
    for (const DepthRow& row : DepthRows(clip)) {
        if (row.depth == depth) continue;
        for (const Span& other : row.spans) {
            if (other.first_frame > span.last_frame || other.last_frame < span.first_frame)
                continue;
            for (const std::size_t index : SpanTags(clip, row.depth, other)) {
                const auto* placement =
                    std::get_if<AfpAnimation::Placement>(&clip.tags[index].body);
                if (placement != nullptr && placement->clip_depth) {
                    return Support::Unexpected(
                        DepthText(row.depth) +
                        " sets a clip depth on those frames, and how a clip depth reaches into a "
                        "sprite is not known");
                }
            }
            if (row.depth < low || row.depth > high) continue;
            return Support::Unexpected(DepthText(row.depth) +
                                       " shows something on those frames between the depths the "
                                       "sprite would put back");
        }
    }
    return {};
}

void PutBack(AfpAnimation::Container& clip, const AfpAnimation::Container& inside, const Span& span,
             const std::vector<DepthRow>& children) {
    const auto length = static_cast<uint32_t>(inside.frames.size());
    for (uint32_t frame = 0; frame < length; frame++) {
        const AfpAnimation::Frame& owner = inside.frames[frame];
        for (uint32_t i = 0; i < owner.tag_count; i++) {
            AfpAnimation::Tag tag = inside.tags[owner.first_tag + i];
            auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
            if (placement != nullptr && placement->end_frame != 0) {
                placement->end_frame =
                    static_cast<uint16_t>(placement->end_frame + span.first_frame);
            }
            InsertTag(clip, span.first_frame + frame, std::move(tag));
        }
    }
    const uint32_t closing = span.last_frame + 1;
    if (closing >= clip.frames.size()) return;
    for (const DepthRow& child : std::views::reverse(children)) {
        if (child.spans.empty() || child.spans.back().last_frame + 1 != length) continue;
        InsertTagFirst(
            clip, closing,
            AfpAnimation::Tag{AfpAnimation::Remove{.unread_word = 0, .depth = child.depth}});
    }
}

void DropUnusedDefinition(AfpAnimation::Animation& animation, uint16_t id) {
    if (Uses(animation.root, id) != 0) return;
    const bool exported = std::ranges::any_of(
        animation.exports, [id](const AfpAnimation::Export& one) { return one.tag == id; });
    if (exported) return;
    for (std::size_t index = 0; index < animation.root.tags.size(); index++) {
        const auto* sprite = std::get_if<AfpAnimation::Sprite>(&animation.root.tags[index].body);
        if (sprite == nullptr || sprite->id != id) continue;
        EraseTag(animation.root, index);
        return;
    }
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

Support::Expected<void, std::string> UngroupSprite(AfpAnimation::Animation& animation,
                                                   ClipId clip_id, uint16_t depth, uint32_t frame) {
    AfpAnimation::Animation edited = animation;
    auto found = RequireClip(edited, clip_id);
    if (!found) return Support::Unexpected(found.error());
    AfpAnimation::Container& clip = **found;
    const std::optional<Span> span = SpanOfDepth(clip, depth, frame);
    if (!span) {
        return Support::Unexpected(DepthText(depth) + " holds nothing on frame " +
                                   std::to_string(frame));
    }
    const std::vector<std::size_t> tags = SpanTags(clip, depth, *span);
    const auto placed = std::ranges::count_if(
        tags, [&clip, depth](std::size_t index) { return IsPlacementOf(clip.tags[index], depth); });
    if (placed != 1) {
        return Support::Unexpected(DepthText(depth) +
                                   " changes after it is placed, which ungrouping would lose");
    }
    const auto& create = std::get<AfpAnimation::Placement>(clip.tags[tags.front()].body);
    const uint16_t id = create.character.value_or(0);
    const AfpAnimation::Container* defined =
        create.character ? SpriteDefinition(edited.root, id) : nullptr;
    if (defined == nullptr)
        return Support::Unexpected(DepthText(depth) + " does not place a sprite there");
    if (!Bare(create)) {
        return Support::Unexpected(
            DepthText(depth) +
            " places its sprite with a transform, colour, name or effect, which ungrouping "
            "would lose");
    }
    const AfpAnimation::Container inside = *defined;
    const uint32_t length = span->last_frame - span->first_frame + 1;
    if (inside.frames.size() != length) {
        return Support::Unexpected("the sprite is " + std::to_string(inside.frames.size()) +
                                   " frames long and " + DepthText(depth) + " shows it for " +
                                   std::to_string(length) + ", so it does not play once through");
    }
    auto contents = CheckContents(inside);
    if (!contents) return Support::Unexpected(contents.error());
    const std::vector<DepthRow> children = DepthRows(inside);
    auto stacking = CheckStacking(clip, depth, *span, children);
    if (!stacking) return Support::Unexpected(stacking.error());

    for (std::size_t i = tags.size(); i > 0; i--)
        EraseTag(clip, tags[i - 1]);
    PutBack(clip, inside, *span, children);
    DropUnusedDefinition(edited, id);
    animation = std::move(edited);
    return {};
}

Support::Expected<uint16_t, std::string> DuplicateSprite(AfpAnimation::Animation& animation,
                                                         uint16_t sprite) {
    const auto defined =
        std::ranges::find_if(animation.root.tags, [sprite](const AfpAnimation::Tag& tag) {
            const auto* found = std::get_if<AfpAnimation::Sprite>(&tag.body);
            return found != nullptr && found->id == sprite;
        });
    if (defined == animation.root.tags.end()) {
        return Support::Unexpected("sprite " + std::to_string(sprite) +
                                   " is not defined in the root");
    }
    const auto index = static_cast<std::size_t>(defined - animation.root.tags.begin());
    const auto frame =
        std::ranges::find_if(animation.root.frames, [index](const AfpAnimation::Frame& one) {
            return index >= one.first_tag && index < one.first_tag + one.tag_count;
        });
    if (frame == animation.root.frames.end()) {
        return Support::Unexpected("sprite " + std::to_string(sprite) +
                                   " is on no frame of the root");
    }
    const auto id = NextCharacterId(animation);
    if (!id) return Support::Unexpected(id.error());
    AfpAnimation::Tag copy = *defined;
    std::get<AfpAnimation::Sprite>(copy.body).id = *id;
    InsertTag(animation.root, static_cast<uint32_t>(frame - animation.root.frames.begin()),
              std::move(copy));
    return *id;
}

Support::Expected<uint16_t, std::string> NewSprite(AfpAnimation::Animation& animation,
                                                   uint32_t frames) {
    if (frames == 0 || frames > std::numeric_limits<uint16_t>::max()) {
        return Support::Unexpected("a sprite holds 1 to 65535 frames, not " +
                                   std::to_string(frames));
    }
    if (animation.root.frames.empty())
        return Support::Unexpected(std::string("the animation has no frame to define a sprite in"));
    const auto id = NextCharacterId(animation);
    if (!id) return Support::Unexpected(id.error());
    AfpAnimation::Sprite sprite{.id = *id, .container = {}};
    sprite.container.frames.assign(frames, AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    InsertTag(animation.root, 0, AfpAnimation::Tag{std::move(sprite)});
    return *id;
}

}
