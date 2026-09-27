#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Document {

struct Rgba {
    int red = 0;
    int green = 0;
    int blue = 0;
    int alpha = 0;

    friend bool operator==(const Rgba&, const Rgba&) = default;
};

[[nodiscard]] bool PicksColour(std::string_view field, std::string_view key_property);

[[nodiscard]] std::optional<Rgba> ColourOfField(std::string_view text);

[[nodiscard]] std::string ColourFieldText(const Rgba& colour);

}
