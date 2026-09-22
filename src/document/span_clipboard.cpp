#include "document/span_clipboard.h"

#include "document/clip.h"
#include "document/span_edit.h"
#include "document/span_tags.h"
#include "document/tags.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr uint16_t kUnusedDepth = 0x3000;

uint32_t FrameHolding(const AfpAnimation::Container& clip, std::size_t index) {
    for (uint32_t frame = 0; frame < clip.frames.size(); frame++) {
        const AfpAnimation::Frame& owner = clip.frames[frame];
        if (index >= owner.first_tag && index < owner.first_tag + owner.tag_count) return frame;
    }
    return 0;
}

bool Reaches(const AfpAnimation::Animation& animation, uint16_t character, uint16_t sprite,
             std::set<uint16_t>& seen) {
    if (character == sprite) return true;
    if (!seen.insert(character).second) return false;
    const AfpAnimation::Container* inside = FindClip(animation, ClipId{.sprite = character});
    if (inside == nullptr) return false;
    for (const AfpAnimation::Tag& tag : inside->tags) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement == nullptr || !placement->character) continue;
        if (Reaches(animation, *placement->character, sprite, seen)) return true;
    }
    return false;
}

Support::Expected<void, std::string> CheckNotInsideItself(const AfpAnimation::Animation& animation,
                                                          ClipId clip, const CopiedSpan& copied) {
    if (!clip.sprite) return {};
    for (const auto& [offset, placement] : copied.placements) {
        if (!placement.character) continue;
        std::set<uint16_t> seen;
        if (Reaches(animation, *placement.character, *clip.sprite, seen)) {
            return Support::Unexpected("sprite " + std::to_string(*clip.sprite) +
                                       " would end up placing itself");
        }
    }
    return {};
}

Support::Expected<void, std::string> CheckRoom(const AfpAnimation::Container& clip,
                                               const CopiedSpan& copied, uint16_t depth,
                                               uint32_t first_frame) {
    if (depth == kUnusedDepth)
        return Support::Unexpected("depth " + std::to_string(depth) + " is reserved by the game");
    const uint32_t last = first_frame + copied.length - 1;
    if (copied.length == 0 || last >= clip.frames.size())
        return Support::Unexpected(std::string("the span does not fit in the clip from there"));
    if (last + 1 > std::numeric_limits<uint16_t>::max())
        return Support::Unexpected(std::string("a placement end frame is a u16"));
    auto free = CheckFree(clip, depth, first_frame, last);
    if (!free) return Support::Unexpected(free.error());
    if (TouchedBetween(clip, depth, first_frame, last + 1)) {
        return Support::Unexpected("depth " + std::to_string(depth) +
                                   " is placed or removed on those frames");
    }
    return {};
}

}

Support::Expected<CopiedSpan, std::string> CopySpan(const AfpAnimation::Animation& animation,
                                                    std::string_view animation_path, ClipId clip,
                                                    uint16_t depth, uint32_t frame) {
    const AfpAnimation::Container* found = FindClip(animation, clip);
    if (found == nullptr) return Support::Unexpected(MissingClipMessage(clip));
    const std::optional<Span> span = SpanOfDepth(*found, depth, frame);
    if (!span) {
        return Support::Unexpected("depth " + std::to_string(depth) + " holds nothing on frame " +
                                   std::to_string(frame));
    }
    CopiedSpan copied{.animation = std::string(animation_path),
                      .length = span->last_frame - span->first_frame + 1,
                      .placements = {},
                      .source = {},
                      .shape_files = {}};
    for (const std::size_t index : SpanTags(*found, depth, *span)) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&found->tags[index].body);
        if (placement == nullptr) continue;
        AfpAnimation::Placement kept = *placement;
        if (kept.end_frame >= span->first_frame)
            kept.end_frame = static_cast<uint16_t>(kept.end_frame - span->first_frame);
        copied.placements.emplace_back(FrameHolding(*found, index) - span->first_frame,
                                       std::move(kept));
    }
    return copied;
}

Support::Expected<Span, std::string> PasteSpan(AfpAnimation::Animation& animation,
                                               std::string_view animation_path, ClipId clip,
                                               const CopiedSpan& copied, uint16_t depth,
                                               uint32_t first_frame) {
    if (copied.animation != animation_path) {
        return Support::Unexpected(std::string(
            "the copied depth places characters of another animation, which this one does not "
            "define"));
    }
    auto inside_itself = CheckNotInsideItself(animation, clip, copied);
    if (!inside_itself) return Support::Unexpected(inside_itself.error());
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    AfpAnimation::Container& target = **found;
    auto room = CheckRoom(target, copied, depth, first_frame);
    if (!room) return Support::Unexpected(room.error());

    for (const auto& [offset, placement] : copied.placements) {
        AfpAnimation::Placement pasted = placement;
        pasted.depth = depth;
        if (pasted.end_frame != 0)
            pasted.end_frame = static_cast<uint16_t>(pasted.end_frame + first_frame);
        InsertTag(target, first_frame + offset, AfpAnimation::Tag{std::move(pasted)});
    }
    const Span placed{.first_frame = first_frame, .last_frame = first_frame + copied.length - 1};
    if (placed.last_frame + 1 < target.frames.size()) {
        InsertTagFirst(target, placed.last_frame + 1,
                       AfpAnimation::Tag{AfpAnimation::Remove{.unread_word = 0, .depth = depth}});
    }
    return placed;
}

}
