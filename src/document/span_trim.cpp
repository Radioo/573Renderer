#include "document/span_trim.h"

#include "document/authored.h"
#include "document/clip.h"
#include "document/keyframes.h"
#include "document/placement_effect.h"
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
#include <string>
#include <utility>
#include <vector>

namespace Document {

namespace {

constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kUseColour = 0x8;
constexpr uint32_t kThreeD = 0x04000000;
constexpr uint32_t kMaxFrame = 0xFFFF;

AfpAnimation::Placement& PlacementAt(AfpAnimation::Container& clip, std::size_t index) {
    return std::get<AfpAnimation::Placement>(clip.tags[index].body);
}

std::string FrameText(uint32_t frame) {
    return "frame " + std::to_string(frame);
}

Support::Expected<void, std::string> CheckFoldable(const AfpAnimation::Placement& create,
                                                   const AfpAnimation::Placement& update,
                                                   uint32_t frame) {
    if (((create.flags ^ update.flags) & kThreeD) != 0) {
        return Support::Unexpected(
            FrameText(frame) + " switches the depth between 2D and 3D, which a trim cannot fold");
    }
    if (update.class_name || update.geometry || update.curves || update.colour_controller ||
        update.grid_controller || update.discarded_words) {
        return Support::Unexpected(FrameText(frame) +
                                   " carries data a trim cannot fold into the first placement");
    }
    return {};
}

void FoldMatrix(AfpAnimation::Placement& create, const AfpAnimation::Placement& update) {
    create.scale = update.scale;
    create.rotate_skew = update.rotate_skew;
    create.translation = update.translation;
    create.short_scale = update.short_scale;
    create.short_rotate_skew = update.short_rotate_skew;
}

void FoldMatrix3d(AfpAnimation::Placement& create, const AfpAnimation::Placement& update) {
    if ((update.flags & kUseMatrix) != 0) create.translation = update.translation;
    if (update.translation_z) create.translation_z = update.translation_z;
    if (update.matrix_3d) create.matrix_3d = update.matrix_3d;
}

void FoldColour(AfpAnimation::Placement& create, const AfpAnimation::Placement& update) {
    create.multiply_colour = update.multiply_colour;
    create.add_colour = update.add_colour;
    create.packed_multiply_colour = update.packed_multiply_colour;
    create.packed_add_colour = update.packed_add_colour;
}

void FoldHeld(AfpAnimation::Placement& create, const AfpAnimation::Placement& update) {
    if (update.character) create.character = update.character;
    if (update.ratio) create.ratio = update.ratio;
    if (update.blend) create.blend = update.blend;
    if (update.origin) create.origin = update.origin;
    if (update.origin_z) create.origin_z = update.origin_z;
    if (update.filters || update.hsv) {
        create.filters = update.filters;
        create.hsv = update.hsv;
    }
    if (!create.name && update.name) create.name = update.name;
}

Support::Expected<void, std::string> Fold(AfpAnimation::Placement& create,
                                          const AfpAnimation::Placement& update, uint32_t frame) {
    auto foldable = CheckFoldable(create, update, frame);
    if (!foldable) return Support::Unexpected(foldable.error());
    if ((create.flags & kThreeD) != 0) {
        FoldMatrix3d(create, update);
    } else if ((update.flags & kUseMatrix) != 0) {
        FoldMatrix(create, update);
    }
    if ((update.flags & kUseColour) != 0) FoldColour(create, update);
    FoldHeld(create, update);
    create.flags |= update.flags & (kUseMatrix | kUseColour);
    return {};
}

std::vector<std::size_t> PlacementsBetween(const AfpAnimation::Container& clip, uint16_t depth,
                                           uint32_t first, uint32_t last) {
    std::vector<std::size_t> found;
    for (const std::size_t index :
         SpanTags(clip, depth, Span{.first_frame = first, .last_frame = last})) {
        if (IsPlacementOf(clip.tags[index], depth)) found.push_back(index);
    }
    return found;
}

uint32_t FrameHolding(const AfpAnimation::Container& clip, std::size_t index) {
    for (uint32_t frame = 0; frame < clip.frames.size(); frame++) {
        const AfpAnimation::Frame& owner = clip.frames[frame];
        if (index >= owner.first_tag && index < owner.first_tag + owner.tag_count) return frame;
    }
    return 0;
}

std::optional<std::size_t> RemoveAt(const AfpAnimation::Container& clip, uint16_t depth,
                                    uint32_t frame) {
    if (frame >= clip.frames.size()) return std::nullopt;
    const AfpAnimation::Frame& owner = clip.frames[frame];
    for (uint32_t i = 0; i < owner.tag_count; i++) {
        const std::size_t index = owner.first_tag + i;
        if (index < clip.tags.size() && IsRemoveOf(clip.tags[index], depth)) return index;
    }
    return std::nullopt;
}

void EraseAll(AfpAnimation::Container& clip, std::vector<std::size_t> indices) {
    std::ranges::sort(indices);
    for (std::size_t i = indices.size(); i > 0; i--)
        EraseTag(clip, indices[i - 1]);
}

Support::Expected<void, std::string> TrimEnd(AfpAnimation::Container& clip, uint16_t depth,
                                             const Span& span, uint32_t last) {
    if (last == span.last_frame) return {};
    std::vector<std::size_t> gone;
    if (last < span.last_frame) gone = PlacementsBetween(clip, depth, last + 1, span.last_frame);
    if (const auto remove = RemoveAt(clip, depth, span.last_frame + 1)) gone.push_back(*remove);
    if (last > span.last_frame) {
        auto free = CheckFree(clip, depth, span.last_frame + 1, last);
        if (!free) return Support::Unexpected(free.error());
    }
    EraseAll(clip, gone);
    if (last + 1 < clip.frames.size()) {
        InsertTagFirst(clip, last + 1,
                       AfpAnimation::Tag{AfpAnimation::Remove{.unread_word = 0, .depth = depth}});
    }
    return {};
}

Support::Expected<void, std::string> StartEarlier(AfpAnimation::Container& clip, uint16_t depth,
                                                  const Span& span, uint32_t first) {
    auto free = CheckFree(clip, depth, first, span.first_frame - 1);
    if (!free) return Support::Unexpected(free.error());
    const std::vector<std::size_t> created =
        PlacementsBetween(clip, depth, span.first_frame, span.first_frame);
    if (created.empty()) return Support::Unexpected(std::string("the span has no first placement"));
    AfpAnimation::Tag create = clip.tags[created.front()];
    EraseTag(clip, created.front());
    InsertTag(clip, first, std::move(create));
    return {};
}

Support::Expected<void, std::string> StartLater(AfpAnimation::Container& clip, uint16_t depth,
                                                const Span& span, uint32_t first) {
    const std::vector<std::size_t> folded =
        PlacementsBetween(clip, depth, span.first_frame, first - 1);
    if (folded.empty()) return Support::Unexpected(std::string("the span has no first placement"));
    AfpAnimation::Placement create = PlacementAt(clip, folded.front());
    for (std::size_t i = 1; i < folded.size(); i++) {
        const std::size_t index = folded[i];
        auto done = Fold(create, PlacementAt(clip, index), FrameHolding(clip, index));
        if (!done) return Support::Unexpected(done.error());
    }
    EraseAll(clip, folded);
    InsertTagFirst(clip, first, AfpAnimation::Tag{std::move(create)});
    return {};
}

Support::Expected<void, std::string> SetEndFrames(AfpAnimation::Container& clip, uint16_t depth,
                                                  const Span& span) {
    if (span.last_frame + 1 > std::numeric_limits<uint16_t>::max())
        return Support::Unexpected(std::string("a placement end frame is a u16"));
    for (const std::size_t index :
         PlacementsBetween(clip, depth, span.first_frame, span.last_frame)) {
        AfpAnimation::Placement& placement = PlacementAt(clip, index);
        if (placement.end_frame != 0)
            placement.end_frame = static_cast<uint16_t>(span.last_frame + 1);
    }
    return {};
}

Support::Expected<void, std::string> CheckShownAlike(const AfpAnimation::Container& before,
                                                     const AfpAnimation::Container& after,
                                                     uint16_t depth, const Span& overlap) {
    if (overlap.first_frame > overlap.last_frame) return {};
    const auto was = ReplayDepth(before, depth, overlap.first_frame, overlap.last_frame);
    const auto now = ReplayDepth(after, depth, overlap.first_frame, overlap.last_frame);
    if (was == now) return {};
    return Support::Unexpected("the trim would change how depth " + std::to_string(depth) +
                               " looks on the frames it keeps");
}

Ease EaseReaching(const Track& track, uint32_t frame) {
    Ease reaching = track.keys.front().ease;
    for (const Keyframe& key : track.keys) {
        if (key.frame >= frame) break;
        reaching = key.ease;
    }
    return reaching;
}

Track Trimmed(const Track& track, const Span& wanted) {
    Track out{.property = track.property, .keys = {}};
    const bool before = track.keys.front().frame < wanted.first_frame;
    const bool after = track.keys.back().frame > wanted.last_frame;
    const bool has_first =
        std::ranges::find(track.keys, wanted.first_frame, &Keyframe::frame) != track.keys.end();
    const bool has_last =
        std::ranges::find(track.keys, wanted.last_frame, &Keyframe::frame) != track.keys.end();
    if (before && !has_first) {
        out.keys.push_back(Keyframe{.frame = wanted.first_frame,
                                    .value = SampleTrack(track, wanted.first_frame),
                                    .ease = EaseReaching(track, wanted.first_frame),
                                    .bezier = {}});
    }
    for (const Keyframe& key : track.keys) {
        if (key.frame >= wanted.first_frame && key.frame <= wanted.last_frame)
            out.keys.push_back(key);
    }
    if (after && !has_last && (out.keys.empty() || out.keys.back().frame < wanted.last_frame)) {
        out.keys.push_back(Keyframe{.frame = wanted.last_frame,
                                    .value = SampleTrack(track, wanted.last_frame),
                                    .ease = Ease::Hold,
                                    .bezier = {}});
    }
    return out;
}

}

Support::Expected<void, std::string> TrimSpan(AfpAnimation::Animation& animation, ClipId clip,
                                              uint16_t depth, uint32_t frame, const Span& wanted) {
    auto found = RequireClip(animation, clip);
    if (!found) return Support::Unexpected(found.error());
    AfpAnimation::Container& target = **found;
    const std::optional<Span> span = SpanOfDepth(target, depth, frame);
    if (!span) {
        return Support::Unexpected("depth " + std::to_string(depth) + " holds nothing on " +
                                   FrameText(frame));
    }
    if (wanted.first_frame > wanted.last_frame)
        return Support::Unexpected(std::string("a span keeps at least one frame"));
    if (wanted.last_frame >= target.frames.size() || wanted.last_frame >= kMaxFrame)
        return Support::Unexpected(std::string("the span would leave the clip's frames"));
    if (wanted == *span) return {};

    AfpAnimation::Container edited = target;
    auto ended = TrimEnd(edited, depth, *span, wanted.last_frame);
    if (!ended) return Support::Unexpected(ended.error());
    const Span ends_right{.first_frame = span->first_frame, .last_frame = wanted.last_frame};
    if (wanted.first_frame < span->first_frame) {
        auto started = StartEarlier(edited, depth, ends_right, wanted.first_frame);
        if (!started) return Support::Unexpected(started.error());
    } else if (wanted.first_frame > span->first_frame) {
        auto started = StartLater(edited, depth, ends_right, wanted.first_frame);
        if (!started) return Support::Unexpected(started.error());
    }
    auto ends = SetEndFrames(edited, depth, wanted);
    if (!ends) return Support::Unexpected(ends.error());
    const std::optional<Span> check = SpanOfDepth(edited, depth, wanted.first_frame);
    if (!check || *check != wanted) {
        return Support::Unexpected("depth " + std::to_string(depth) +
                                   " would run into another span there");
    }
    const Span overlap{.first_frame = std::max(span->first_frame, wanted.first_frame),
                       .last_frame = std::min(span->last_frame, wanted.last_frame)};
    auto alike = CheckShownAlike(target, edited, depth, overlap);
    if (!alike) return Support::Unexpected(alike.error());
    target = std::move(edited);
    return {};
}

Support::Expected<void, std::string> TrimOwnedSpan(AfpAnimation::Animation& animation,
                                                   AuthoredDepth& authored, const Span& wanted) {
    AuthoredDepth trimmed = authored;
    auto cut = TrimAuthored(trimmed, wanted);
    if (!cut) return Support::Unexpected(cut.error());
    AfpAnimation::Animation edited = animation;
    auto done = TrimSpan(edited, authored.clip, authored.depth, authored.first_frame, wanted);
    if (!done) return Support::Unexpected(done.error());
    const auto baked = BakedFor(edited, trimmed);
    if (!baked) return Support::Unexpected(baked.error());
    auto written = WriteAuthored(edited, trimmed, *baked);
    if (!written) return Support::Unexpected(written.error());
    animation = std::move(edited);
    authored = std::move(trimmed);
    return {};
}

Support::Expected<void, std::string> TrimAuthored(AuthoredDepth& authored, const Span& wanted) {
    if (wanted.first_frame > wanted.last_frame)
        return Support::Unexpected(std::string("a span keeps at least one frame"));
    AuthoredDepth trimmed = authored;
    trimmed.first_frame = wanted.first_frame;
    trimmed.last_frame = wanted.last_frame;
    for (Track& track : trimmed.tracks) {
        if (track.keys.empty()) continue;
        track = Trimmed(track, wanted);
    }
    authored = std::move(trimmed);
    return {};
}

}
