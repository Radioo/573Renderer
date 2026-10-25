#include "document/span_edit.h"

#include "document/authored.h"
#include "document/clip.h"
#include "document/keyframes.h"
#include "document/span_tags.h"
#include "document/tags.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr uint32_t kMaxFrame = 0xFFFF;
constexpr uint16_t kUnusedDepth = 0x3000;

struct Taken {
    uint32_t frame = 0;
    AfpAnimation::Tag tag;
};

uint32_t FrameOf(const AfpAnimation::Container& clip, std::size_t index) {
    for (uint32_t frame = 0; frame < clip.frames.size(); frame++) {
        const AfpAnimation::Frame& owner = clip.frames[frame];
        if (index >= owner.first_tag && index < owner.first_tag + owner.tag_count) return frame;
    }
    return 0;
}

std::vector<Taken> TakeSpan(AfpAnimation::Container& clip, uint16_t depth, const Span& span) {
    const std::vector<std::size_t> indices = SpanTags(clip, depth, span);
    std::vector<Taken> taken;
    taken.reserve(indices.size());
    for (const std::size_t index : indices)
        taken.push_back(Taken{.frame = FrameOf(clip, index), .tag = clip.tags[index]});
    for (std::size_t i = indices.size(); i > 0; i--)
        EraseTag(clip, indices[i - 1]);
    return taken;
}

Support::Expected<void, std::string> PlaceShifted(AfpAnimation::Container& clip, uint16_t depth,
                                                  const std::vector<Taken>& taken,
                                                  const Span& moved, int64_t by) {
    bool closed = false;
    for (const Taken& one : taken) {
        const auto frame = static_cast<uint32_t>(static_cast<int64_t>(one.frame) + by);
        AfpAnimation::Tag tag = one.tag;
        if (IsRemoveOf(tag, depth)) {
            if (frame >= clip.frames.size()) continue;
            InsertTagFirst(clip, frame, std::move(tag));
            closed = true;
            continue;
        }
        auto& placement = std::get<AfpAnimation::Placement>(tag.body);
        if (placement.end_frame != 0) {
            const int64_t end = static_cast<int64_t>(placement.end_frame) + by;
            if (end < 0 || std::cmp_greater(end, std::numeric_limits<uint16_t>::max()))
                return Support::Unexpected(std::string("a placement end frame is a u16"));
            placement.end_frame = static_cast<uint16_t>(end);
        }
        InsertTag(clip, frame, std::move(tag));
    }
    const uint32_t closing = moved.last_frame + 1;
    if (!closed && closing < clip.frames.size()) {
        InsertTagFirst(clip, closing,
                       AfpAnimation::Tag{AfpAnimation::Remove{.unread_word = 0, .depth = depth}});
    }
    return {};
}

uint32_t FramesAway(const Span& span, uint32_t frame) {
    if (frame < span.first_frame) return span.first_frame - frame;
    if (frame > span.last_frame) return frame - span.last_frame;
    return 0;
}

std::string NothingAt(uint16_t depth, uint32_t frame) {
    return "depth " + std::to_string(depth) + " holds nothing on frame " + std::to_string(frame);
}

Support::Expected<Span, std::string> FreeTarget(const AfpAnimation::Container& clip, uint16_t depth,
                                                uint32_t frame, uint16_t to) {
    if (to == kUnusedDepth)
        return Support::Unexpected("depth " + std::to_string(to) + " is reserved by the game");
    const std::optional<Span> span = SpanOfDepth(clip, depth, frame);
    if (!span) return Support::Unexpected(NothingAt(depth, frame));
    if (to == depth) return *span;
    auto free = CheckFree(clip, to, span->first_frame, span->last_frame);
    if (!free) return Support::Unexpected(free.error());
    if (TouchedBetween(clip, to, span->first_frame, span->last_frame + 1)) {
        return Support::Unexpected("depth " + std::to_string(to) +
                                   " is placed or removed while this span is on the stage");
    }
    return *span;
}

uint32_t FrameOfTag(const AfpAnimation::Container& clip, std::size_t index) {
    for (uint32_t frame = 0; frame < clip.frames.size(); frame++) {
        const AfpAnimation::Frame& owner = clip.frames[frame];
        if (index >= owner.first_tag && index < owner.first_tag + owner.tag_count) return frame;
    }
    return 0;
}

}

std::optional<Span> SpanOfDepth(const AfpAnimation::Container& clip, uint16_t depth,
                                uint32_t frame) {
    for (const DepthRow& row : DepthRows(clip)) {
        if (row.depth != depth) continue;
        for (const Span& span : row.spans) {
            if (frame >= span.first_frame && frame <= span.last_frame) return span;
        }
    }
    return std::nullopt;
}

std::optional<Span> NearestSpan(const AfpAnimation::Container& clip, uint16_t depth,
                                uint32_t frame) {
    std::optional<Span> nearest;
    uint32_t best = std::numeric_limits<uint32_t>::max();
    for (const DepthRow& row : DepthRows(clip)) {
        if (row.depth != depth) continue;
        for (const Span& span : row.spans) {
            const uint32_t away = FramesAway(span, frame);
            if (away >= best) continue;
            best = away;
            nearest = span;
        }
    }
    return nearest;
}

Support::Expected<Span, std::string> MoveSpan(AfpAnimation::Animation& animation, ClipId clip,
                                              uint16_t depth, uint32_t frame, int64_t by) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    AfpAnimation::Container& target = **found;
    const std::optional<Span> span = SpanOfDepth(target, depth, frame);
    if (!span) {
        return Support::Unexpected("depth " + std::to_string(depth) + " holds nothing on frame " +
                                   std::to_string(frame));
    }
    const int64_t first = static_cast<int64_t>(span->first_frame) + by;
    const int64_t last = static_cast<int64_t>(span->last_frame) + by;
    if (first < 0 || std::cmp_greater_equal(last, target.frames.size()) ||
        std::cmp_greater_equal(last, kMaxFrame)) {
        return Support::Unexpected(std::string("the depth would leave the clip's frames"));
    }
    const Span moved{.first_frame = static_cast<uint32_t>(first),
                     .last_frame = static_cast<uint32_t>(last)};
    if (by == 0) return moved;

    AfpAnimation::Container edited = target;
    const std::vector<Taken> taken = TakeSpan(edited, depth, *span);
    auto free = CheckFree(edited, depth, moved.first_frame, moved.last_frame);
    if (!free) return Support::Unexpected(free.error());
    auto placed = PlaceShifted(edited, depth, taken, moved, by);
    if (!placed) return Support::Unexpected(placed.error());
    const std::optional<Span> check = SpanOfDepth(edited, depth, moved.first_frame);
    if (!check || *check != moved) {
        return Support::Unexpected("depth " + std::to_string(depth) +
                                   " would run into its next span there");
    }
    target = std::move(edited);
    return moved;
}

Support::Expected<void, std::string> ChangeSpanDepth(AfpAnimation::Animation& animation,
                                                     ClipId clip, uint16_t depth, uint32_t frame,
                                                     uint16_t to) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    AfpAnimation::Container& target = **found;
    if (to == depth) {
        if (!SpanOfDepth(target, depth, frame)) return Support::Unexpected(NothingAt(depth, frame));
        return {};
    }
    auto span = FreeTarget(target, depth, frame, to);
    if (!span) return Support::Unexpected(span.error());
    for (const std::size_t index : SpanTags(target, depth, *span)) {
        auto& body = target.tags[index].body;
        if (auto* placement = std::get_if<AfpAnimation::Placement>(&body)) placement->depth = to;
        if (auto* remove = std::get_if<AfpAnimation::Remove>(&body)) remove->depth = to;
    }
    return {};
}

std::optional<uint16_t> FreeDepthAbove(const AfpAnimation::Container& clip, uint16_t depth,
                                       uint32_t frame) {
    if (!SpanOfDepth(clip, depth, frame)) return std::nullopt;
    for (uint32_t above = depth + 1U; above <= std::numeric_limits<uint16_t>::max(); above++) {
        if (FreeTarget(clip, depth, frame, static_cast<uint16_t>(above)))
            return static_cast<uint16_t>(above);
    }
    return std::nullopt;
}

Support::Expected<void, std::string> DuplicateSpan(AfpAnimation::Animation& animation, ClipId clip,
                                                   uint16_t depth, uint32_t frame, uint16_t to) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    AfpAnimation::Container& target = **found;
    auto span = FreeTarget(target, depth, frame, to);
    if (!span) return Support::Unexpected(span.error());
    if (to == depth)
        return Support::Unexpected(std::string("a span is duplicated onto another depth"));

    std::vector<std::pair<uint32_t, AfpAnimation::Tag>> copies;
    for (const std::size_t index : SpanTags(target, depth, *span)) {
        AfpAnimation::Tag copy = target.tags[index];
        if (auto* placement = std::get_if<AfpAnimation::Placement>(&copy.body))
            placement->depth = to;
        if (auto* remove = std::get_if<AfpAnimation::Remove>(&copy.body)) remove->depth = to;
        copies.emplace_back(FrameOfTag(target, index), std::move(copy));
    }
    for (auto& [at, copy] : copies) {
        if (std::holds_alternative<AfpAnimation::Remove>(copy.body)) {
            InsertTagFirst(target, at, std::move(copy));
        } else {
            InsertTag(target, at, std::move(copy));
        }
    }
    return {};
}

Support::Expected<void, std::string> ShiftAuthored(AuthoredDepth& authored, int64_t by) {
    const int64_t first = static_cast<int64_t>(authored.first_frame) + by;
    const int64_t last = static_cast<int64_t>(authored.last_frame) + by;
    if (first < 0 || std::cmp_greater(last, std::numeric_limits<uint32_t>::max()))
        return Support::Unexpected(std::string("the owned range would leave the clip"));
    AuthoredDepth moved = authored;
    moved.first_frame = static_cast<uint32_t>(first);
    moved.last_frame = static_cast<uint32_t>(last);
    for (Track& track : moved.tracks) {
        for (Keyframe& key : track.keys)
            key.frame = static_cast<uint32_t>(static_cast<int64_t>(key.frame) + by);
    }
    authored = std::move(moved);
    return {};
}

}
