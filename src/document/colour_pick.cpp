#include "document/colour_pick.h"

#include "document/field_values.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Document {

namespace {

constexpr std::array<std::string_view, 2> kColourFields{"Multiply colour", "Add colour"};
constexpr std::string_view kKeyValue = "Keyframe value";
constexpr int64_t kChannelMax = 255;

bool IsColour(std::string_view name) {
    return std::ranges::find(kColourFields, name) != kColourFields.end();
}

int Channel(int64_t value) {
    return static_cast<int>(std::clamp<int64_t>(value, 0, kChannelMax));
}

}

bool PicksColour(std::string_view field, std::string_view key_property) {
    if (field == kKeyValue) return IsColour(key_property);
    return IsColour(field);
}

std::optional<Rgba> ColourOfField(std::string_view text) {
    const auto numbers = Numbers(text, 4);
    if (!numbers) return std::nullopt;
    return Rgba{.red = Channel((*numbers)[0]),
                .green = Channel((*numbers)[1]),
                .blue = Channel((*numbers)[2]),
                .alpha = Channel((*numbers)[3])};
}

std::string ColourFieldText(const Rgba& colour) {
    return Join({std::to_string(colour.red), std::to_string(colour.green),
                 std::to_string(colour.blue), std::to_string(colour.alpha)});
}

}
