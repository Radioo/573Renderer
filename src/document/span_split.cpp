#include "document/span_split.h"

#include "document/clip.h"
#include "document/span_edit.h"
#include "document/span_tags.h"
#include "document/span_trim.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace Document {

namespace {

Support::Expected<void, std::string> CheckSplittable(const AfpAnimation::Animation& animation,
                                                     const AfpAnimation::Container& clip,
                                                     uint16_t depth, uint32_t frame,
                                                     const Span& span) {
    const std::string named = "depth " + std::to_string(depth);
    if (frame == span.first_frame) {
        return Support::Unexpected(named + " starts on frame " + std::to_string(frame) +
                                   ", so there is nothing before it to split from");
    }
    const std::optional<uint16_t> character = CharacterOn(clip, depth, span, frame);
    if (!character || !IsStillCharacter(animation, *character)) {
        return Support::Unexpected(
            named + " shows a character on frame " + std::to_string(frame) +
            " that is not an image or a shape, so a new object there would start again");
    }
    return {};
}

void Relabel(AfpAnimation::Container& clip, uint16_t from, const Span& span, uint16_t to) {
    for (const std::size_t index : SpanTags(clip, from, span)) {
        auto& body = clip.tags[index].body;
        if (auto* placement = std::get_if<AfpAnimation::Placement>(&body)) placement->depth = to;
        if (auto* remove = std::get_if<AfpAnimation::Remove>(&body)) remove->depth = to;
    }
}

}

std::optional<uint16_t> CharacterOn(const AfpAnimation::Container& clip, uint16_t depth,
                                    const Span& span, uint32_t frame) {
    std::optional<uint16_t> shown;
    for (uint32_t at = span.first_frame; at <= frame && at < clip.frames.size(); at++) {
        const AfpAnimation::Frame& owner = clip.frames[at];
        for (std::size_t tag = 0; tag < owner.tag_count; tag++) {
            const std::size_t index = owner.first_tag + tag;
            if (index >= clip.tags.size()) break;
            const auto* placement = std::get_if<AfpAnimation::Placement>(&clip.tags[index].body);
            if (placement != nullptr && placement->depth == depth && placement->character)
                shown = placement->character;
        }
    }
    return shown;
}

bool IsStillCharacter(const AfpAnimation::Animation& animation, uint16_t character) {
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        if (const auto* image = std::get_if<AfpAnimation::Image>(&tag.body)) {
            if (image->id == character) return true;
        }
        if (const auto* shape = std::get_if<AfpAnimation::Shape>(&tag.body)) {
            if (shape->id == character) return true;
        }
    }
    return false;
}

Support::Expected<void, std::string> SplitSpan(AfpAnimation::Animation& animation, ClipId clip,
                                               uint16_t depth, uint32_t frame) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    const AfpAnimation::Container& target = **found;
    const std::optional<Span> span = SpanOfDepth(target, depth, frame);
    if (!span) {
        return Support::Unexpected("depth " + std::to_string(depth) + " holds nothing on frame " +
                                   std::to_string(frame));
    }
    auto splittable = CheckSplittable(animation, target, depth, frame, *span);
    if (!splittable) return Support::Unexpected(splittable.error());
    return SplitSpanRestarting(animation, clip, depth, *span, frame);
}

Support::Expected<void, std::string> SplitSpanRestarting(AfpAnimation::Animation& animation,
                                                         ClipId clip, uint16_t depth,
                                                         const Span& span, uint32_t frame) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    const std::optional<uint16_t> scratch = UnusedDepth(**found);
    if (!scratch) return Support::Unexpected(std::string("every depth of the clip is in use"));

    const Span before{.first_frame = span.first_frame, .last_frame = frame - 1};
    const Span after{.first_frame = frame, .last_frame = span.last_frame};
    AfpAnimation::Animation edited = animation;
    auto copied = DuplicateSpan(edited, clip, depth, frame, *scratch);
    if (!copied) return Support::Unexpected(copied.error());
    auto started = TrimSpan(edited, clip, *scratch, frame, after);
    if (!started) return Support::Unexpected(started.error());
    auto ended = TrimSpan(edited, clip, depth, frame, before);
    if (!ended) return Support::Unexpected(ended.error());
    AfpAnimation::Container& split = **RequireClip(edited, clip);
    Relabel(split, *scratch, after, depth);
    animation = std::move(edited);
    return {};
}

}
