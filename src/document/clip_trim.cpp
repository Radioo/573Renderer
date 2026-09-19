#include "document/clip_trim.h"

#include "document/clip.h"
#include "document/frame_edit.h"
#include "document/span_split.h"
#include "document/span_trim.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Document {

namespace {

Support::Expected<void, std::string> CheckKept(const AfpAnimation::Container& clip,
                                               const Span& kept) {
    const auto frames = static_cast<uint32_t>(clip.frames.size());
    if (kept.first_frame > kept.last_frame || kept.last_frame >= frames) {
        return Support::Unexpected("frames " + std::to_string(kept.first_frame) + " to " +
                                   std::to_string(kept.last_frame) + " are not frames of the clip");
    }
    if (kept.first_frame == 0 && kept.last_frame + 1 == frames)
        return Support::Unexpected(std::string("the clip already has exactly those frames"));
    return {};
}

bool StartsAgain(const AfpAnimation::Animation& animation, const AfpAnimation::Container& clip,
                 uint16_t depth, const Span& span, uint32_t frame) {
    const std::optional<uint16_t> character = CharacterOn(clip, depth, span, frame);
    return character && !IsStillCharacter(animation, *character);
}

Support::Expected<std::size_t, std::string> TrimSpans(AfpAnimation::Animation& edited,
                                                      const AfpAnimation::Animation& original,
                                                      const AfpAnimation::Container& before,
                                                      ClipId clip, const Span& kept) {
    std::size_t restarted = 0;
    for (const DepthRow& row : DepthRows(before)) {
        for (const Span& span : row.spans) {
            if (span.last_frame < kept.first_frame || span.first_frame > kept.last_frame) {
                auto removed = RemoveDepth(edited, clip, row.depth, span.first_frame);
                if (!removed) return Support::Unexpected(removed.error());
                continue;
            }
            const Span wanted{.first_frame = std::max(span.first_frame, kept.first_frame),
                              .last_frame = std::min(span.last_frame, kept.last_frame)};
            if (span.first_frame < kept.first_frame &&
                StartsAgain(original, before, row.depth, span, kept.first_frame)) {
                restarted++;
            }
            auto trimmed = TrimSpan(edited, clip, row.depth, span.first_frame, wanted);
            if (!trimmed) return Support::Unexpected(trimmed.error());
        }
    }
    return restarted;
}

}

Support::Expected<std::size_t, std::string> TrimClipToFrames(AfpAnimation::Animation& animation,
                                                             ClipId clip, const Span& kept) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    auto fits = CheckKept(**found, kept);
    if (!fits) return Support::Unexpected(fits.error());

    AfpAnimation::Animation edited = animation;
    auto restarted = TrimSpans(edited, animation, **found, clip, kept);
    if (!restarted) return Support::Unexpected(restarted.error());
    const auto frames = static_cast<uint32_t>((**found).frames.size());
    for (uint32_t frame = frames - 1; frame > kept.last_frame; frame--) {
        auto removed = RemoveFrame(edited, clip, frame);
        if (!removed) return Support::Unexpected(removed.error());
    }
    for (uint32_t frame = 0; frame < kept.first_frame; frame++) {
        auto removed = RemoveFrame(edited, clip, 0);
        if (!removed) return Support::Unexpected(removed.error());
    }
    animation = std::move(edited);
    return restarted;
}

}
