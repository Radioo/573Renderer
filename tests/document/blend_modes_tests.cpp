#include <catch2/catch_test_macros.hpp>

#include "document/blend_modes.h"

#include <algorithm>
#include <array>
#include <cstdint>

TEST_CASE("Every blend mode the renderer maps has a name to choose") {
    const auto named = [](uint8_t value) {
        const auto modes = Document::BlendModes();
        return std::ranges::find(modes, value, &Document::BlendMode::value) != modes.end();
    };
    for (const uint8_t value : std::array<uint8_t, 9>{0, 3, 4, 5, 6, 8, 9, 0x46, 0x4F})
        CHECK(named(value));
    CHECK(Document::BlendName(0) == "Normal");
    CHECK(Document::BlendName(3) == "Multiply");
    CHECK(Document::BlendName(9) == "Subtractive");
}

TEST_CASE("A blend value the game may still carry is named by its number") {
    CHECK(Document::BlendName(12) == "Mode 12");
    CHECK(Document::BlendName(255) == "Mode 255");
    const auto modes = Document::BlendModes();
    CHECK(std::ranges::find(modes, uint8_t{12}, &Document::BlendMode::value) == modes.end());
}
