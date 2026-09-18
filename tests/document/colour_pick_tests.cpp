#include <catch2/catch_test_macros.hpp>

#include "document/colour_pick.h"

#include <optional>

TEST_CASE("Colour rows are the unpacked colours and their keyframe values") {
    CHECK(Document::PicksColour("Multiply colour", ""));
    CHECK(Document::PicksColour("Add colour", ""));
    CHECK(Document::PicksColour("Keyframe value", "Multiply colour"));
    CHECK(Document::PicksColour("Keyframe value", "Add colour"));
    CHECK_FALSE(Document::PicksColour("Keyframe value", "Translation"));
    CHECK_FALSE(Document::PicksColour("Packed multiply colour", ""));
    CHECK_FALSE(Document::PicksColour("Translation", "Multiply colour"));
}

TEST_CASE("A colour cell reads as four channels held to what a picker shows") {
    CHECK(Document::ColourOfField("10, 20, 30, 40") == Document::Rgba{10, 20, 30, 40});
    CHECK(Document::ColourOfField("-20, 300, 255, 0") == Document::Rgba{0, 255, 255, 0});
    CHECK_FALSE(Document::ColourOfField("").has_value());
    CHECK_FALSE(Document::ColourOfField("1, 2, 3").has_value());
    CHECK(Document::ColourFieldText(Document::Rgba{1, 2, 3, 255}) == "1, 2, 3, 255");
}
