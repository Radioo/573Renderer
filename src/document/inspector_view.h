#pragma once

#include "document/authored.h"
#include "document/clip.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

enum class ViewUnit : uint8_t { Pixels, Percent, Degrees, Colour, Channels };

enum class Keying : uint8_t { Baked, NotAnimated, Animated, KeyedHere };

struct ViewRow {
    std::string label;
    std::vector<double> values;
    ViewUnit unit = ViewUnit::Pixels;
    uint32_t set_on = 0;
    Keying keying = Keying::Baked;
};

struct PlacementView {
    std::vector<ViewRow> transform;
    std::vector<ViewRow> colours;
};

[[nodiscard]] std::optional<PlacementView> ViewPlacement(const AfpAnimation::Container& clip,
                                                         uint16_t depth, uint32_t frame,
                                                         const AuthoredDepth* owned);

[[nodiscard]] Support::Expected<void, std::string>
SetViewedBaked(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth, uint32_t frame,
               std::string_view label, const std::vector<double>& values);

[[nodiscard]] std::optional<std::string> ViewTrack(const AuthoredDepth& owned,
                                                   std::string_view label);

[[nodiscard]] Support::Expected<void, std::string>
SetViewedOwned(AuthoredDepth& authored, const BakedDepth& baked, uint32_t frame,
               std::string_view label, const std::vector<double>& values);

}
