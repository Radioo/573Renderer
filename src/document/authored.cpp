#include "document/authored.h"

#include "document/keyframes.h"
#include "document/placement_edit.h"
#include "document/placement_values.h"
#include "document/script_source.h"
#include "document/tags.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr uint32_t kUpdateExisting = 0x1;

struct Placed {
    uint32_t frame = 0;
    std::size_t tag = 0;
};

std::vector<Placed> SpanPlacements(const AfpAnimation::Container& clip, uint16_t depth,
                                   uint32_t first, uint32_t last) {
    std::vector<Placed> found;
    for (uint32_t frame = first; frame <= last && frame < clip.frames.size(); frame++) {
        const AfpAnimation::Frame& owner = clip.frames[frame];
        for (uint32_t i = 0; i < owner.tag_count; i++) {
            const std::size_t index = owner.first_tag + i;
            if (index >= clip.tags.size()) break;
            const auto* placement = std::get_if<AfpAnimation::Placement>(&clip.tags[index].body);
            if (placement != nullptr && placement->depth == depth)
                found.push_back(Placed{.frame = frame, .tag = index});
        }
    }
    return found;
}

const AfpAnimation::Placement& PlacementAt(const AfpAnimation::Container& clip,
                                           const Placed& placed) {
    return std::get<AfpAnimation::Placement>(clip.tags[placed.tag].body);
}

bool CoversFrame(const Track& track, uint32_t frame) {
    return !track.keys.empty() && frame >= track.keys.front().frame &&
           frame <= track.keys.back().frame;
}

bool WritesFrame(const Track& track, uint32_t frame) {
    if (!CoversFrame(track, frame)) return false;
    std::size_t after = 0;
    while (after < track.keys.size() && track.keys[after].frame < frame)
        after++;
    if (after < track.keys.size() && track.keys[after].frame == frame) return true;
    return track.keys[after - 1].ease != Ease::Hold;
}

Support::Expected<void, std::string> CheckSpan(const AfpAnimation::Container& clip, uint16_t depth,
                                               const std::vector<Placed>& placements) {
    const AfpAnimation::Placement& create = PlacementAt(clip, placements.front());
    for (std::size_t i = 1; i < placements.size(); i++) {
        const std::string where = "frame " + std::to_string(placements[i].frame);
        const AfpAnimation::Placement& later = PlacementAt(clip, placements[i]);
        if ((later.flags & kUpdateExisting) == 0) {
            return Support::Unexpected(where + " places depth " + std::to_string(depth) +
                                       " again instead of updating it");
        }
        const std::vector<std::string> parts = UnanimatableParts(later);
        if (!parts.empty()) {
            return Support::Unexpected(where + " changes " + parts.front() +
                                       ", which is not something a keyframe can hold");
        }
        if (later.end_frame != create.end_frame) {
            return Support::Unexpected(where + " ends depth " + std::to_string(depth) +
                                       " somewhere else than its first frame does");
        }
        const AfpAnimation::Placement& first_update = PlacementAt(clip, placements[1]);
        if (later.flags != first_update.flags ||
            later.extended_flags != first_update.extended_flags) {
            return Support::Unexpected(where + " updates depth " + std::to_string(depth) +
                                       " with flags of its own");
        }
    }
    return {};
}

std::pair<uint32_t, uint32_t> SpanAround(const AfpAnimation::Container& clip, uint16_t depth,
                                         uint32_t frame) {
    uint32_t first = frame;
    while (first > 0 && LivePlacementTag(clip, depth, first - 1))
        first--;
    uint32_t last = frame;
    while (last + 1 < clip.frames.size() && LivePlacementTag(clip, depth, last + 1))
        last++;
    return {first, last};
}

Support::Expected<void, std::string> ApplyScript(AfpAnimation::Animation& animation,
                                                 const std::string& source, const BakedDepth& baked,
                                                 AfpAnimation::Placement& placement) {
    if (!baked.create.clip_actions || baked.create.clip_actions->events.empty()) {
        return Support::Unexpected(
            std::string("this depth carries no script to replace, and the editor does not know "
                        "what would make the game run a new one"));
    }
    auto code = CompileScript(animation, source);
    if (!code) return Support::Unexpected(code.error());
    AfpAnimation::ClipActions actions = *baked.create.clip_actions;
    actions.events.resize(1);
    actions.events.front().bytecode = std::move(*code);
    placement.clip_actions = std::move(actions);
    return {};
}

Support::Expected<std::vector<Placed>, std::string> SpanOf(const AfpAnimation::Container& clip,
                                                           uint16_t depth, uint32_t frame) {
    if (frame >= clip.frames.size())
        return Support::Unexpected("the clip has no frame " + std::to_string(frame));
    if (!LivePlacementTag(clip, depth, frame)) {
        return Support::Unexpected("depth " + std::to_string(depth) + " holds nothing on frame " +
                                   std::to_string(frame));
    }
    const auto [first, last] = SpanAround(clip, depth, frame);
    std::vector<Placed> placements = SpanPlacements(clip, depth, first, last);
    if (placements.empty() || placements.front().frame != first ||
        (PlacementAt(clip, placements.front()).flags & kUpdateExisting) != 0) {
        return Support::Unexpected("depth " + std::to_string(depth) +
                                   " does not start with a placement on frame " +
                                   std::to_string(first));
    }
    auto shaped = CheckSpan(clip, depth, placements);
    if (!shaped) return Support::Unexpected(shaped.error());
    return placements;
}

BakedDepth BakedOf(const AfpAnimation::Container& clip, const std::vector<Placed>& placements) {
    BakedDepth baked;
    baked.create = PlacementAt(clip, placements.front());
    ClearAnimatableProperties(baked.create);
    if (placements.size() > 1) {
        const AfpAnimation::Placement& update = PlacementAt(clip, placements[1]);
        baked.update_flags = update.flags;
        baked.update_extended_flags = update.extended_flags;
    } else {
        baked.update_flags = kUpdateExisting;
    }
    for (std::size_t i = 1; i < placements.size(); i++) {
        bool carries = false;
        for (const std::string_view property : AnimatableProperties()) {
            carries =
                carries || ReadProperty(PlacementAt(clip, placements[i]), property).has_value();
        }
        if (!carries) baked.blank_frames.push_back(placements[i].frame);
    }
    return baked;
}

}

Support::Expected<OwnedDepth, std::string> OwnDepth(const AfpAnimation::Animation& animation,
                                                    std::string_view animation_path, uint16_t depth,
                                                    uint32_t frame) {
    const AfpAnimation::Container& clip = animation.root;
    auto placements = SpanOf(clip, depth, frame);
    if (!placements) return Support::Unexpected(placements.error());

    const auto [first, last] = SpanAround(clip, depth, frame);
    OwnedDepth owned;
    owned.authored.animation = std::string(animation_path);
    owned.authored.depth = depth;
    owned.authored.first_frame = first;
    owned.authored.last_frame = last;
    owned.baked = BakedOf(clip, *placements);

    for (const std::string_view property : AnimatableProperties()) {
        Track track{.property = std::string(property), .keys = {}};
        for (const Placed& placed : *placements) {
            std::optional<std::vector<int64_t>> value =
                ReadProperty(PlacementAt(clip, placed), property);
            if (!value) continue;
            track.keys.push_back(Keyframe{.frame = placed.frame,
                                          .value = std::move(*value),
                                          .ease = Ease::Hold,
                                          .bezier = {}});
        }
        if (!track.keys.empty()) owned.authored.tracks.push_back(std::move(track));
    }
    return owned;
}

Support::Expected<BakedDepth, std::string> BakedFor(const AfpAnimation::Animation& animation,
                                                    const AuthoredDepth& authored) {
    auto placements = SpanOf(animation.root, authored.depth, authored.first_frame);
    if (!placements) return Support::Unexpected(placements.error());
    return BakedOf(animation.root, *placements);
}

Support::Expected<std::vector<std::pair<uint32_t, AfpAnimation::Placement>>, std::string>
AuthoredPlacements(const AuthoredDepth& authored, const BakedDepth& baked) {
    if (authored.first_frame > authored.last_frame)
        return Support::Unexpected(std::string("the authored range runs backwards"));
    for (const Track& track : authored.tracks) {
        auto shaped = CheckTrack(track);
        if (!shaped) return Support::Unexpected(shaped.error());
        if (track.keys.front().frame < authored.first_frame ||
            track.keys.back().frame > authored.last_frame) {
            return Support::Unexpected("the keyframes of " + track.property +
                                       " fall outside the authored range");
        }
    }

    std::vector<std::pair<uint32_t, AfpAnimation::Placement>> out;
    for (uint32_t frame = authored.first_frame; frame <= authored.last_frame; frame++) {
        AfpAnimation::Placement placement;
        if (frame == authored.first_frame) {
            placement = baked.create;
        } else {
            placement.flags = baked.update_flags | kUpdateExisting;
            placement.extended_flags = baked.update_extended_flags;
            placement.depth = authored.depth;
            placement.end_frame = baked.create.end_frame;
        }
        bool wrote = false;
        for (const Track& track : authored.tracks) {
            if (!WritesFrame(track, frame)) continue;
            const std::vector<int64_t> value = SampleTrack(track, frame);
            auto written = WriteProperty(placement, track.property, value);
            if (!written) return Support::Unexpected(written.error());
            wrote = true;
        }
        const bool blank = std::ranges::find(baked.blank_frames, frame) != baked.blank_frames.end();
        if (frame == authored.first_frame || wrote || blank)
            out.emplace_back(frame, std::move(placement));
    }
    if (out.empty()) return Support::Unexpected(std::string("the authored depth places nothing"));
    return out;
}

Support::Expected<void, std::string> WriteAuthored(AfpAnimation::Animation& animation,
                                                   const AuthoredDepth& authored,
                                                   const BakedDepth& baked) {
    AfpAnimation::Container& clip = animation.root;
    if (authored.last_frame >= clip.frames.size())
        return Support::Unexpected("the clip has no frame " + std::to_string(authored.last_frame));
    auto placements = AuthoredPlacements(authored, baked);
    if (!placements) return Support::Unexpected(placements.error());
    if (authored.script) {
        auto scripted = ApplyScript(animation, *authored.script, baked, placements->front().second);
        if (!scripted) return Support::Unexpected(scripted.error());
    }

    const std::vector<Placed> standing =
        SpanPlacements(clip, authored.depth, authored.first_frame, authored.last_frame);
    std::vector<bool> written(placements->size(), false);
    std::vector<std::size_t> gone;
    for (const Placed& placed : standing) {
        std::size_t at = 0;
        while (at < placements->size() && (*placements)[at].first != placed.frame)
            at++;
        if (at == placements->size() || written[at]) {
            gone.push_back(placed.tag);
            continue;
        }
        clip.tags[placed.tag].body = (*placements)[at].second;
        written[at] = true;
    }
    for (std::size_t i = gone.size(); i > 0; i--)
        EraseTag(clip, gone[i - 1]);

    for (std::size_t at = 0; at < placements->size(); at++) {
        if (written[at]) continue;
        InsertTag(clip, (*placements)[at].first,
                  AfpAnimation::Tag{std::move((*placements)[at].second)});
    }
    return {};
}

}
