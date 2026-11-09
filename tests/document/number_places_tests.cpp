#include <catch2/catch_test_macros.hpp>

#include "document/animation_strings.h"
#include "document/inputs.h"
#include "document/number_places.h"
#include "formats/afp_animation.h"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

constexpr uint32_t kPlaceCharacter = 0x2;
constexpr uint32_t kPlaceWithMatrix = 0x6;
constexpr int32_t kUnitsPerPixel = 20;

AfpAnimation::Animation Anchored() {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    const AfpAnimation::StringId named = Document::InternString(animation, "deadpoint");
    const AfpAnimation::StringId glyph = Document::InternString(animation, "dead_0");

    AfpAnimation::Placement inside;
    inside.flags = kPlaceCharacter;
    inside.depth = 1;
    inside.end_frame = 1;
    inside.character = uint16_t{3};
    AfpAnimation::Container digit;
    digit.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 1});
    digit.tags = {AfpAnimation::Tag{inside}};

    AfpAnimation::Placement anchor;
    anchor.flags = kPlaceWithMatrix;
    anchor.depth = 4;
    anchor.end_frame = 1;
    anchor.character = uint16_t{10};
    anchor.name = named;
    anchor.translation = std::array<int32_t, 2>{100 * kUnitsPerPixel, 60 * kUnitsPerPixel};

    animation.root.tags = {
        AfpAnimation::Tag{AfpAnimation::Image{.id = 3, .name = glyph}},
        AfpAnimation::Tag{AfpAnimation::Sprite{.id = 10, .container = digit}},
        AfpAnimation::Tag{anchor},
    };
    animation.root.frames = {AfpAnimation::Frame{.first_tag = 0, .tag_count = 3}};
    return animation;
}

Document::NumberSpread Spread(uint32_t places,
                              Document::NumberGrows grows = Document::NumberGrows::Left) {
    return Document::NumberSpread{.clip = {},
                                  .name = "deadpoint",
                                  .depth = 4,
                                  .frame = 0,
                                  .places = places,
                                  .advance = 24,
                                  .grows = grows};
}

std::map<std::string, AfpAnimation::Placement> Named(const AfpAnimation::Animation& animation) {
    std::map<std::string, AfpAnimation::Placement> found;
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement == nullptr || !placement->name) continue;
        found.emplace(Document::StringText(animation, *placement->name), *placement);
    }
    return found;
}

}

TEST_CASE("Spreading a named place writes the digit places the number machinery already reads") {
    AfpAnimation::Animation animation = Anchored();

    REQUIRE(Document::SpreadIntoPlaces(animation, Spread(4)).has_value());

    const std::map<std::string, AfpAnimation::Placement> places = Named(animation);
    REQUIRE(places.size() == 4);
    CHECK(places.contains("deadpoint_0001"));
    CHECK(places.contains("deadpoint_0010"));
    CHECK(places.contains("deadpoint_0100"));
    CHECK(places.contains("deadpoint_1000"));
    CHECK_FALSE(places.contains("deadpoint"));
}

TEST_CASE("Each digit place keeps the character and steps one glyph to the left") {
    AfpAnimation::Animation animation = Anchored();

    REQUIRE(Document::SpreadIntoPlaces(animation, Spread(4)).has_value());

    const std::map<std::string, AfpAnimation::Placement> places = Named(animation);
    for (const auto& [name, placement] : places) {
        CHECK(placement.character == std::optional<uint16_t>{uint16_t{10}});
    }
    const auto x = [&places](const std::string& name) {
        return (*places.at(name).translation)[0] / kUnitsPerPixel;
    };
    CHECK(x("deadpoint_0001") == 100);
    CHECK(x("deadpoint_0010") == 76);
    CHECK(x("deadpoint_0100") == 52);
    CHECK(x("deadpoint_1000") == 28);
    CHECK((*places.at("deadpoint_1000").translation)[1] / kUnitsPerPixel == 60);
}

TEST_CASE("A spread place is read back as one number of that many digits") {
    AfpAnimation::Animation animation = Anchored();
    REQUIRE(Document::SpreadIntoPlaces(animation, Spread(3)).has_value());

    const Document::InputSurface surface = Document::Inputs(animation, {{uint16_t{3}, "dead_0"}});
    std::vector<std::string> images;
    for (int digit = 0; digit < 10; digit++) {
        std::string one = "dead_";
        one += static_cast<char>('0' + digit);
        images.push_back(std::move(one));
    }
    const std::vector<Document::InputNumber> numbers = Document::Numbers(surface, images);

    REQUIRE(numbers.size() == 1);
    CHECK(numbers.front().stem == "deadpoint");
    REQUIRE(numbers.front().weights.size() == 3);
    CHECK(numbers.front().weights.front() == 100);
}

TEST_CASE("A spread of fewer than two places or onto itself is refused") {
    AfpAnimation::Animation animation = Anchored();

    CHECK_FALSE(Document::SpreadIntoPlaces(animation, Spread(1)).has_value());
    CHECK_FALSE(Document::SpreadIntoPlaces(animation, Spread(99)).has_value());
    Document::NumberSpread flat = Spread(4);
    flat.advance = 0;
    CHECK_FALSE(Document::SpreadIntoPlaces(animation, flat).has_value());
    CHECK(Named(animation).contains("deadpoint"));
}

TEST_CASE("A name the clip does not carry is refused instead of writing half a number") {
    AfpAnimation::Animation animation = Anchored();
    Document::NumberSpread missing = Spread(4);
    missing.name = "nothing_here";

    CHECK_FALSE(Document::SpreadIntoPlaces(animation, missing).has_value());
}

TEST_CASE("Only the create tag of each place carries the name, as the shipped files do") {
    AfpAnimation::Animation animation = Anchored();
    AfpAnimation::Placement update;
    update.flags = kPlaceCharacter | 0x1;
    update.depth = 4;
    update.translation = std::array<int32_t, 2>{50 * kUnitsPerPixel, 60 * kUnitsPerPixel};
    animation.root.tags.push_back(AfpAnimation::Tag{update});
    animation.root.frames.front().tag_count = 4;

    REQUIRE(Document::SpreadIntoPlaces(animation, Spread(2)).has_value());

    std::map<std::string, int> counted;
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement == nullptr || !placement->name) continue;
        counted[Document::StringText(animation, *placement->name)]++;
    }
    REQUIRE(counted.size() == 2);
    CHECK(counted.at("deadpoint_01") == 1);
    CHECK(counted.at("deadpoint_10") == 1);
}

TEST_CASE("Every keyframe of a copied place moves with it, not just the first") {
    AfpAnimation::Animation animation = Anchored();
    AfpAnimation::Placement update;
    update.flags = kPlaceCharacter | 0x1;
    update.depth = 4;
    update.translation = std::array<int32_t, 2>{50 * kUnitsPerPixel, 60 * kUnitsPerPixel};
    animation.root.tags.push_back(AfpAnimation::Tag{update});
    animation.root.frames.front().tag_count = 4;

    REQUIRE(Document::SpreadIntoPlaces(animation, Spread(2)).has_value());

    std::vector<int32_t> moved;
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement == nullptr || placement->depth == 4 || !placement->translation) continue;
        moved.push_back((*placement->translation)[0] / kUnitsPerPixel);
    }
    REQUIRE(moved.size() == 2);
    CHECK(moved[0] == 76);
    CHECK(moved[1] == 26);
}

TEST_CASE("A keyframe that sets no position keeps inheriting one instead of being pinned") {
    AfpAnimation::Animation animation = Anchored();
    AfpAnimation::Placement faded;
    faded.flags = kPlaceCharacter | 0x1;
    faded.depth = 4;
    animation.root.tags.push_back(AfpAnimation::Tag{faded});
    animation.root.frames.front().tag_count = 4;

    REQUIRE(Document::SpreadIntoPlaces(animation, Spread(2)).has_value());

    int untouched = 0;
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement == nullptr || placement->depth == 4) continue;
        if (!placement->translation) untouched++;
    }
    CHECK(untouched == 1);
}

TEST_CASE("A number that grows right keeps the anchor as its most significant place") {
    AfpAnimation::Animation animation = Anchored();

    REQUIRE(
        Document::SpreadIntoPlaces(animation, Spread(4, Document::NumberGrows::Right)).has_value());

    const std::map<std::string, AfpAnimation::Placement> places = Named(animation);
    const auto x = [&places](const std::string& name) {
        return (*places.at(name).translation)[0] / kUnitsPerPixel;
    };
    CHECK(x("deadpoint_1000") == 100);
    CHECK(x("deadpoint_0100") == 124);
    CHECK(x("deadpoint_0010") == 148);
    CHECK(x("deadpoint_0001") == 172);
}

TEST_CASE("Place names follow the power of ten convention the surface already uses") {
    CHECK(Document::PlaceName("score", 4, 0) == std::string("score_0001"));
    CHECK(Document::PlaceName("score", 4, 3) == std::string("score_1000"));
    CHECK(Document::PlaceName("score", 2, 1) == std::string("score_10"));
}
