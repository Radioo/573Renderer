#include "document/clip_extract.h"

#include "document/clip.h"
#include "document/frame_edit.h"
#include "document/span_split.h"
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
#include <vector>

namespace Document {

namespace {

Support::Expected<void, std::string> CheckRange(const AfpAnimation::Container& clip,
                                                const Span& cut) {
    if (cut.first_frame > cut.last_frame || cut.last_frame >= clip.frames.size()) {
        return Support::Unexpected("frames " + std::to_string(cut.first_frame) + " to " +
                                   std::to_string(cut.last_frame) + " are not frames of the clip");
    }
    return {};
}

Support::Expected<void, std::string> CheckCut(const AfpAnimation::Container& clip,
                                              const Span& cut) {
    auto range = CheckRange(clip, cut);
    if (!range) return range;
    if (cut.first_frame == 0 && cut.last_frame + 1 == clip.frames.size()) {
        return Support::Unexpected(
            std::string("extracting every frame would leave the clip empty"));
    }
    return {};
}

bool ClosesOnCut(const Span& span, const Span& cut) {
    return span.last_frame + 1 == cut.first_frame ||
           (span.first_frame < cut.first_frame && span.last_frame >= cut.first_frame &&
            span.last_frame <= cut.last_frame);
}

bool ShowsMovingCharacter(const AfpAnimation::Animation& animation,
                          const AfpAnimation::Container& clip, uint16_t depth, const Span& span,
                          uint32_t frame) {
    const std::optional<uint16_t> character = CharacterOn(clip, depth, span, frame);
    return character && !IsStillCharacter(animation, *character);
}

Support::Expected<bool, std::string> CutSpan(AfpAnimation::Animation& edited,
                                             const AfpAnimation::Animation& original,
                                             const AfpAnimation::Container& before, ClipId clip,
                                             uint16_t depth, const Span& span, const Span& cut) {
    const bool runs_in = span.first_frame < cut.first_frame;
    const bool runs_out = span.last_frame > cut.last_frame;
    Support::Expected<void, std::string> done;
    bool moving = false;
    if (!runs_in && !runs_out) {
        done = RemoveDepth(edited, clip, depth, span.first_frame);
    } else if (runs_in && runs_out) {
        moving = ShowsMovingCharacter(original, before, depth, span, cut.first_frame);
        done =
            CarryUpdates(**RequireClip(edited, clip), depth, cut.first_frame, cut.last_frame + 1);
    } else if (runs_in) {
        done = TrimSpan(edited, clip, depth, span.first_frame,
                        Span{.first_frame = span.first_frame, .last_frame = cut.first_frame - 1});
    } else {
        moving = ShowsMovingCharacter(original, before, depth, span, cut.last_frame + 1);
        done = TrimSpan(edited, clip, depth, span.first_frame,
                        Span{.first_frame = cut.last_frame + 1, .last_frame = span.last_frame});
    }
    if (!done) return Support::Unexpected(done.error());
    return moving;
}

bool Overlaps(const Span& span, const Span& cut) {
    return span.last_frame >= cut.first_frame && span.first_frame <= cut.last_frame;
}

Support::Expected<bool, std::string> LiftSpan(AfpAnimation::Animation& edited,
                                              const AfpAnimation::Animation& original,
                                              const AfpAnimation::Container& before, ClipId clip,
                                              uint16_t depth, const Span& span, const Span& cut) {
    const bool runs_in = span.first_frame < cut.first_frame;
    const bool runs_out = span.last_frame > cut.last_frame;
    const Span kept_before{.first_frame = span.first_frame, .last_frame = cut.first_frame - 1};
    Support::Expected<void, std::string> done;
    if (runs_in && runs_out) {
        done = SplitSpanRestarting(edited, clip, depth, span, cut.last_frame + 1);
        if (done) done = TrimSpan(edited, clip, depth, span.first_frame, kept_before);
    } else if (runs_in) {
        done = TrimSpan(edited, clip, depth, span.first_frame, kept_before);
    } else if (runs_out) {
        done = TrimSpan(edited, clip, depth, span.first_frame,
                        Span{.first_frame = cut.last_frame + 1, .last_frame = span.last_frame});
    } else {
        done = RemoveDepth(edited, clip, depth, span.first_frame);
    }
    if (!done) return Support::Unexpected(done.error());
    return runs_out && ShowsMovingCharacter(original, before, depth, span, cut.last_frame + 1);
}

}

Support::Expected<std::size_t, std::string> ExtractFrames(AfpAnimation::Animation& animation,
                                                          ClipId clip, const Span& cut) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    const AfpAnimation::Container& before = **found;
    auto fits = CheckCut(before, cut);
    if (!fits) return Support::Unexpected(fits.error());

    AfpAnimation::Animation edited = animation;
    std::size_t moving = 0;
    std::vector<uint16_t> closing;
    for (const DepthRow& row : DepthRows(before)) {
        for (const Span& span : row.spans) {
            if (ClosesOnCut(span, cut)) closing.push_back(row.depth);
            if (!Overlaps(span, cut)) continue;
            auto cut_span = CutSpan(edited, animation, before, clip, row.depth, span, cut);
            if (!cut_span) return Support::Unexpected(cut_span.error());
            if (*cut_span) moving++;
        }
    }
    for (uint32_t frame = cut.first_frame; frame <= cut.last_frame; frame++) {
        auto removed = RemoveFrame(edited, clip, cut.first_frame);
        if (!removed) return Support::Unexpected(removed.error());
    }
    AfpAnimation::Container& kept = **RequireClip(edited, clip);
    if (cut.first_frame < kept.frames.size()) {
        for (const uint16_t depth : closing) {
            InsertTagFirst(
                kept, cut.first_frame,
                AfpAnimation::Tag{AfpAnimation::Remove{.unread_word = 0, .depth = depth}});
        }
    }
    animation = std::move(edited);
    return moving;
}

Support::Expected<std::size_t, std::string> LiftFrames(AfpAnimation::Animation& animation,
                                                       ClipId clip, const Span& cut) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    const AfpAnimation::Container& before = **found;
    auto fits = CheckRange(before, cut);
    if (!fits) return Support::Unexpected(fits.error());

    AfpAnimation::Animation edited = animation;
    std::size_t restarting = 0;
    bool lifted = false;
    for (const DepthRow& row : DepthRows(before)) {
        for (const Span& span : row.spans) {
            if (!Overlaps(span, cut)) continue;
            auto lift = LiftSpan(edited, animation, before, clip, row.depth, span, cut);
            if (!lift) return Support::Unexpected(lift.error());
            if (*lift) restarting++;
            lifted = true;
        }
    }
    if (!lifted) {
        return Support::Unexpected("nothing is shown on frames " + std::to_string(cut.first_frame) +
                                   " to " + std::to_string(cut.last_frame));
    }
    animation = std::move(edited);
    return restarting;
}

}
