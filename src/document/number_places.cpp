#include "document/number_places.h"

#include "document/animation_strings.h"
#include "document/clip.h"
#include "document/span_edit.h"
#include "document/stage_move.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Document {

namespace {

std::vector<AfpAnimation::Placement*> PlacementsNamed(AfpAnimation::Container& clip,
                                                      const AfpAnimation::Animation& animation,
                                                      const std::string& name) {
    std::vector<AfpAnimation::Placement*> found;
    for (AfpAnimation::Tag& tag : clip.tags) {
        auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement == nullptr || !placement->name) continue;
        if (StringText(animation, *placement->name) == name) found.push_back(placement);
    }
    return found;
}

constexpr uint32_t kUpdateExisting = 0x1;

std::vector<AfpAnimation::Placement*> PlacementsOnDepth(AfpAnimation::Container& clip,
                                                        uint16_t depth) {
    std::vector<AfpAnimation::Placement*> found;
    for (AfpAnimation::Tag& tag : clip.tags) {
        auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement != nullptr && placement->depth == depth) found.push_back(placement);
    }
    return found;
}

std::vector<AfpAnimation::Placement*> Creates(const std::vector<AfpAnimation::Placement*>& all) {
    std::vector<AfpAnimation::Placement*> found;
    for (AfpAnimation::Placement* placement : all) {
        if ((placement->flags & kUpdateExisting) == 0) found.push_back(placement);
    }
    return found;
}

Support::Expected<void, std::string> Rename(AfpAnimation::Animation& animation,
                                            const std::vector<AfpAnimation::Placement*>& places,
                                            const std::string& name) {
    if (places.empty()) return Support::Unexpected(std::string("nothing to name ") + name);
    const AfpAnimation::StringId held = InternString(animation, name);
    for (AfpAnimation::Placement* placement : places)
        placement->name = held;
    return {};
}

Support::Expected<void, std::string> Slide(const std::vector<AfpAnimation::Placement*>& places,
                                           double by) {
    for (AfpAnimation::Placement* placement : places) {
        const bool positioned =
            placement->translation.has_value() || (placement->flags & kUpdateExisting) == 0;
        if (!positioned) continue;
        auto shifted = ShiftPlacement(*placement, StageOffset{.x = by, .y = 0});
        if (!shifted) return Support::Unexpected(shifted.error());
    }
    return {};
}

Support::Expected<void, std::string> CopyPlace(AfpAnimation::Animation& animation,
                                               const NumberSpread& spread, uint32_t anchored,
                                               uint32_t at) {
    auto clip = RequireClip(animation, spread.clip);
    if (!clip) return Support::Unexpected(clip.error());
    const std::optional<uint16_t> above = FreeDepthAbove(**clip, spread.depth, spread.frame);
    if (!above) {
        return Support::Unexpected(std::string("there is no free depth for digit place ") +
                                   std::to_string(at + 1));
    }
    auto copied = DuplicateSpan(animation, spread.clip, spread.depth, spread.frame, *above);
    if (!copied) return Support::Unexpected(copied.error());

    auto again = RequireClip(animation, spread.clip);
    if (!again) return Support::Unexpected(again.error());
    const std::vector<AfpAnimation::Placement*> made = PlacementsOnDepth(**again, *above);
    auto marked = Rename(animation, Creates(made), PlaceName(spread.name, spread.places, at));
    if (!marked) return Support::Unexpected(marked.error());
    const double step = static_cast<double>(anchored) - static_cast<double>(at);
    return Slide(made, spread.advance * step);
}

}

std::string PlaceName(const std::string& stem, uint32_t places, uint32_t at) {
    std::string digits(places, '0');
    digits[places - 1 - at] = '1';
    return stem + "_" + digits;
}

Support::Expected<void, std::string> SpreadIntoPlaces(AfpAnimation::Animation& animation,
                                                      const NumberSpread& spread) {
    if (spread.places < 2 || spread.places > kMostNumberPlaces) {
        return Support::Unexpected(std::string("a number has 2 to ") +
                                   std::to_string(kMostNumberPlaces) + " digit places");
    }
    if (spread.advance <= 0) {
        return Support::Unexpected(std::string("the digits would sit on top of each other"));
    }
    auto held = RequireClip(animation, spread.clip);
    if (!held) return Support::Unexpected(held.error());

    const uint32_t anchored = spread.grows == NumberGrows::Left ? 0 : spread.places - 1;
    auto named = Rename(animation, PlacementsNamed(**held, animation, spread.name),
                        PlaceName(spread.name, spread.places, anchored));
    if (!named) return Support::Unexpected(named.error());

    for (uint32_t at = 0; at < spread.places; at++) {
        if (at == anchored) continue;
        auto made = CopyPlace(animation, spread, anchored, at);
        if (!made) return Support::Unexpected(made.error());
    }
    return {};
}

}
