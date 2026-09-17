#include "document/authored.h"

#include "document/clip.h"
#include "document/keyframes.h"
#include "document/placement_edit.h"
#include "document/placement_effect.h"
#include "document/placement_values.h"
#include "document/property_groups.h"
#include "document/script_source.h"
#include "document/tags.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
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
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kUseColour = 0x8;
constexpr uint32_t kControlBits = kUseMatrix | kUseColour;
constexpr uint32_t kThreeD = 0x04000000;
constexpr std::string_view kCharacter = "Character";

struct StartableProperty {
    std::string_view property;
    std::string_view twin;
};

constexpr std::array<StartableProperty, 5> kStartable{{
    {.property = "Scale", .twin = "Short scale"},
    {.property = "Rotate skew", .twin = "Short rotate skew"},
    {.property = "Translation", .twin = {}},
    {.property = "Multiply colour", .twin = "Packed multiply colour"},
    {.property = "Add colour", .twin = "Packed add colour"},
}};

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

Support::Expected<void, std::string> CheckStepped(const Track& track) {
    if (!PropertyIsStepped(track.property)) return {};
    const bool eased =
        std::ranges::any_of(track.keys, [](const Keyframe& key) { return key.ease != Ease::Hold; });
    if (!eased) return {};
    return Support::Unexpected(track.property +
                               " jumps from one keyframe to the next and only holds");
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

uint32_t NeededControls(const AfpAnimation::Placement& placement) {
    uint32_t bits = 0;
    if (placement.scale || placement.rotate_skew || placement.translation ||
        placement.short_scale || placement.short_rotate_skew) {
        bits |= kUseMatrix;
    }
    if (placement.multiply_colour || placement.add_colour || placement.packed_multiply_colour ||
        placement.packed_add_colour) {
        bits |= kUseColour;
    }
    return bits;
}

Support::Expected<void, std::string> CheckControls(const AfpAnimation::Placement& placement,
                                                   uint32_t frame) {
    const uint32_t missing = NeededControls(placement) & ~placement.flags;
    if (missing == 0) return {};
    const std::string field = (missing & kUseMatrix) != 0 ? "matrix" : "colour";
    return Support::Unexpected("frame " + std::to_string(frame) + " carries a " + field +
                               " the game would not apply, which own could not give back");
}

bool Applies(uint32_t flags, PropertyGroup group) {
    return group != PropertyGroup::None && (flags & GroupBit(group)) != 0;
}

bool ThreeD(const AfpAnimation::Placement& placement) {
    return (placement.flags & kThreeD) != 0;
}

bool IsExplicit(const BakedDepth& baked, uint32_t frame, std::string_view property) {
    return std::ranges::any_of(baked.explicit_identities, [&](const ExplicitIdentity& one) {
        return one.frame == frame && one.property == property;
    });
}

bool SwapsCharacter(const AfpAnimation::Container& clip, const std::vector<Placed>& placements) {
    return std::any_of(placements.begin() + 1, placements.end(), [&](const Placed& placed) {
        return PlacementAt(clip, placed).character.has_value();
    });
}

bool Tracked(const AfpAnimation::Container& clip, const std::vector<Placed>& placements,
             std::string_view property) {
    if (property == kCharacter && !SwapsCharacter(clip, placements)) return false;
    return std::ranges::any_of(placements, [&](const Placed& placed) {
        return ReadProperty(PlacementAt(clip, placed), property).has_value();
    });
}

Support::Expected<void, std::string> CheckSpan(const AfpAnimation::Container& clip, uint16_t depth,
                                               const std::vector<Placed>& placements) {
    const AfpAnimation::Placement& create = PlacementAt(clip, placements.front());
    auto applied = CheckControls(create, placements.front().frame);
    if (!applied) return Support::Unexpected(applied.error());
    for (std::size_t i = 1; i < placements.size(); i++) {
        const std::string where = "frame " + std::to_string(placements[i].frame);
        const AfpAnimation::Placement& later = PlacementAt(clip, placements[i]);
        auto later_applied = CheckControls(later, placements[i].frame);
        if (!later_applied) return Support::Unexpected(later_applied.error());
        if ((later.flags & kUpdateExisting) == 0) {
            return Support::Unexpected(where + " places depth " + std::to_string(depth) +
                                       " again instead of updating it");
        }
        const std::vector<std::string> parts = UnanimatableParts(later);
        if (!parts.empty()) {
            return Support::Unexpected(where + " changes " + parts.front() +
                                       ", which is not something a keyframe can hold");
        }
        if (ThreeD(later) != ThreeD(create)) {
            return Support::Unexpected(where + " switches depth " + std::to_string(depth) +
                                       " between 2D and 3D");
        }
        if (later.end_frame != create.end_frame) {
            return Support::Unexpected(where + " ends depth " + std::to_string(depth) +
                                       " somewhere else than its first frame does");
        }
        const AfpAnimation::Placement& first_update = PlacementAt(clip, placements[1]);
        if ((later.flags & ~kControlBits) != (first_update.flags & ~kControlBits) ||
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
    if (!SwapsCharacter(clip, placements))
        baked.create.character = PlacementAt(clip, placements.front()).character;
    if (placements.size() > 1) {
        const AfpAnimation::Placement& update = PlacementAt(clip, placements[1]);
        baked.update_flags = update.flags & ~kControlBits;
        baked.update_extended_flags = update.extended_flags;
    } else {
        baked.update_flags = kUpdateExisting;
    }
    const bool three_d = ThreeD(baked.create);
    for (const Placed& placed : placements) {
        const AfpAnimation::Placement& placement = PlacementAt(clip, placed);
        for (const std::string_view property : AnimatableProperties()) {
            if (!Applies(placement.flags, GroupOf(property, three_d))) continue;
            const std::optional<std::vector<int64_t>> value = ReadProperty(placement, property);
            if (value && value == IdentityOf(property)) {
                baked.explicit_identities.push_back(
                    ExplicitIdentity{.frame = placed.frame, .property = std::string(property)});
            }
        }
    }
    for (std::size_t i = 1; i < placements.size(); i++) {
        const AfpAnimation::Placement& update = PlacementAt(clip, placements[i]);
        const uint32_t extra = update.flags & kControlBits & ~NeededControls(update);
        if (extra != 0) {
            baked.extra_controls.push_back(
                FrameControls{.frame = placements[i].frame, .bits = extra});
        }
        bool carries = false;
        for (const std::string_view property : AnimatableProperties()) {
            carries =
                carries || ReadProperty(PlacementAt(clip, placements[i]), property).has_value();
        }
        if (!carries) baked.blank_frames.push_back(placements[i].frame);
    }
    return baked;
}

uint32_t AppliedGroups(const AuthoredDepth& authored, const BakedDepth& baked, uint32_t frame) {
    uint32_t bits = 0;
    if (frame == authored.first_frame) bits |= baked.create.flags & kControlBits;
    const auto extra = std::ranges::find(baked.extra_controls, frame, &FrameControls::frame);
    if (extra != baked.extra_controls.end()) bits |= extra->bits;
    const bool three_d = ThreeD(baked.create);
    for (const Track& track : authored.tracks) {
        const PropertyGroup group = GroupOf(track.property, three_d);
        if (group != PropertyGroup::None && WritesFrame(track, frame)) bits |= GroupBit(group);
    }
    return bits;
}

std::optional<std::vector<int64_t>> WrittenValue(const BakedDepth& baked, const Track& track,
                                                 uint32_t frame, uint32_t applied) {
    const PropertyGroup group = GroupOf(track.property, ThreeD(baked.create));
    if (group == PropertyGroup::None) {
        if (!WritesFrame(track, frame)) return std::nullopt;
        return SampleTrack(track, frame);
    }
    if (!Applies(applied, group) || track.keys.empty() || frame < track.keys.front().frame)
        return std::nullopt;
    std::vector<int64_t> value = SampleTrack(track, frame);
    if (value == IdentityOf(track.property) && !IsExplicit(baked, frame, track.property))
        return std::nullopt;
    return value;
}

}

Support::Expected<OwnedDepth, std::string> OwnDepth(const AfpAnimation::Animation& animation,
                                                    ClipId clip_id, std::string_view animation_path,
                                                    uint16_t depth, uint32_t frame) {
    const AfpAnimation::Container* found = FindClip(animation, clip_id);
    if (found == nullptr) return Support::Unexpected(MissingClipMessage(clip_id));
    const AfpAnimation::Container& clip = *found;
    auto placements = SpanOf(clip, depth, frame);
    if (!placements) return Support::Unexpected(placements.error());

    const auto [first, last] = SpanAround(clip, depth, frame);
    OwnedDepth owned;
    owned.authored.animation = std::string(animation_path);
    owned.authored.depth = depth;
    owned.authored.first_frame = first;
    owned.authored.last_frame = last;
    owned.authored.clip = clip_id;
    owned.baked = BakedOf(clip, *placements);

    const bool three_d = ThreeD(owned.baked.create);
    for (const std::string_view property : AnimatableProperties()) {
        if (!Tracked(clip, *placements, property)) continue;
        const PropertyGroup group = GroupOf(property, three_d);
        Track track{.property = std::string(property), .keys = {}};
        for (const Placed& placed : *placements) {
            const AfpAnimation::Placement& placement = PlacementAt(clip, placed);
            std::optional<std::vector<int64_t>> value = ReadProperty(placement, property);
            if (!value && Applies(placement.flags, group)) value = IdentityOf(property);
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
    const AfpAnimation::Container* clip = FindClip(animation, authored.clip);
    if (clip == nullptr) return Support::Unexpected(MissingClipMessage(authored.clip));
    auto placements = SpanOf(*clip, authored.depth, authored.first_frame);
    if (!placements) return Support::Unexpected(placements.error());
    return BakedOf(*clip, *placements);
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
        auto stepped = CheckStepped(track);
        if (!stepped) return Support::Unexpected(stepped.error());
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
        const uint32_t applied = AppliedGroups(authored, baked, frame);
        bool wrote = false;
        for (const Track& track : authored.tracks) {
            const std::optional<std::vector<int64_t>> value =
                WrittenValue(baked, track, frame, applied);
            if (!value) continue;
            auto written = WriteProperty(placement, track.property, *value);
            if (!written) return Support::Unexpected(written.error());
            wrote = true;
        }
        placement.flags |= NeededControls(placement) | applied;
        const bool blank = std::ranges::find(baked.blank_frames, frame) != baked.blank_frames.end();
        if (frame == authored.first_frame || wrote || applied != 0 || blank)
            out.emplace_back(frame, std::move(placement));
    }
    if (out.empty()) return Support::Unexpected(std::string("the authored depth places nothing"));
    return out;
}

std::vector<std::string> PropertiesToAdd(const AuthoredDepth& authored, const BakedDepth& baked) {
    const bool three_d = ThreeD(baked.create);
    std::vector<std::string> out;
    for (const StartableProperty& startable : kStartable) {
        if (GroupOf(startable.property, three_d) == PropertyGroup::None) continue;
        const bool animated = std::ranges::any_of(authored.tracks, [&](const Track& track) {
            return track.property == startable.property || track.property == startable.twin;
        });
        if (!animated) out.emplace_back(startable.property);
    }
    return out;
}

Support::Expected<void, std::string> AddTrack(AuthoredDepth& authored, const BakedDepth& baked,
                                              std::string_view property) {
    const std::vector<std::string> startable = PropertiesToAdd(authored, baked);
    if (std::ranges::find(startable, property) == startable.end()) {
        return Support::Unexpected(std::string(property) +
                                   " is not a property this depth can start animating");
    }
    const std::optional<std::vector<int64_t>> identity = IdentityOf(property);
    if (!identity) return Support::Unexpected(std::string(property) + " has no resting value");
    authored.tracks.push_back(Track{
        .property = std::string(property),
        .keys = {Keyframe{
            .frame = authored.first_frame, .value = *identity, .ease = Ease::Hold, .bezier = {}}}});
    return {};
}

AppliedState KeyedState(const AuthoredDepth& authored, const BakedDepth& baked, uint32_t frame) {
    AfpAnimation::Placement placement;
    placement.flags = kUseMatrix | kUseColour | (baked.create.flags & kThreeD);
    for (const Track& track : authored.tracks) {
        if (track.keys.empty() || frame < track.keys.front().frame) continue;
        const std::vector<int64_t> value = SampleTrack(track, frame);
        if (value == IdentityOf(track.property) && !IsExplicit(baked, frame, track.property))
            continue;
        auto written = WriteProperty(placement, track.property, value);
        if (!written) continue;
    }
    AppliedState state;
    ApplyPlacement(state, placement);
    return state;
}

Support::Expected<void, std::string> CheckDrawnAsKeyed(const AfpAnimation::Container& clip,
                                                       const AuthoredDepth& authored,
                                                       const BakedDepth& baked) {
    for (const auto& [frame, shown] :
         ReplayDepth(clip, authored.depth, authored.first_frame, authored.last_frame)) {
        if (shown == KeyedState(authored, baked, frame)) continue;
        return Support::Unexpected("frame " + std::to_string(frame) + " would not show depth " +
                                   std::to_string(authored.depth) + " the way its keyframes say");
    }
    return {};
}

Support::Expected<void, std::string> WriteAuthored(AfpAnimation::Animation& animation,
                                                   const AuthoredDepth& authored,
                                                   const BakedDepth& baked) {
    auto target = RequireClip(animation, authored.clip);
    if (!target) return Support::Unexpected(target.error());
    AfpAnimation::Container& clip = **target;
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
    return CheckDrawnAsKeyed(clip, authored, baked);
}

}
