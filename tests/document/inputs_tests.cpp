#include <catch2/catch_test_macros.hpp>

#include "document/animation_strings.h"
#include "document/clip.h"
#include "document/inputs.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

AfpAnimation::Placement Places(uint16_t depth, uint16_t character, AfpAnimation::StringId name) {
    AfpAnimation::Placement placement;
    placement.depth = depth;
    placement.character = character;
    placement.name = name;
    return placement;
}

AfpAnimation::Placement Bare(uint16_t depth) {
    AfpAnimation::Placement placement;
    placement.depth = depth;
    return placement;
}

AfpAnimation::Container OneFrame(const std::vector<AfpAnimation::Tag>& tags) {
    AfpAnimation::Container clip;
    AfpAnimation::Frame frame;
    frame.tag_count = static_cast<uint32_t>(tags.size());
    clip.frames.push_back(frame);
    clip.tags = tags;
    return clip;
}

AfpAnimation::Animation Result() {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    const AfpAnimation::StringId ones = Document::InternString(animation, "score_this_0001");
    const AfpAnimation::StringId tens = Document::InternString(animation, "score_this_0010");
    const AfpAnimation::StringId lamp = Document::InternString(animation, "clear_lamp");
    const AfpAnimation::StringId loop = Document::InternString(animation, "loop");
    const AfpAnimation::StringId glyph = Document::InternString(animation, "num_0");

    AfpAnimation::Placement glyph_at;
    glyph_at.depth = 1;
    glyph_at.character = uint16_t{3};
    const AfpAnimation::Container digit = OneFrame({AfpAnimation::Tag{glyph_at}});
    AfpAnimation::Container lamps;
    lamps.frames = {AfpAnimation::Frame{}, AfpAnimation::Frame{}, AfpAnimation::Frame{}};
    lamps.labels = {AfpAnimation::Label{.frame = 2, .name = loop}};

    animation.root.tags = {
        AfpAnimation::Tag{AfpAnimation::Image{.id = 3, .name = glyph}},
        AfpAnimation::Tag{AfpAnimation::Sprite{.id = 10, .container = digit}},
        AfpAnimation::Tag{AfpAnimation::Sprite{.id = 11, .container = lamps}},
        AfpAnimation::Tag{Places(4, 10, ones)},
        AfpAnimation::Tag{Places(5, 10, tens)},
        AfpAnimation::Tag{Places(6, 11, lamp)},
    };
    animation.root.frames = {AfpAnimation::Frame{.first_tag = 0, .tag_count = 6}};
    return animation;
}

std::map<uint16_t, std::string> Glyphs() {
    return {{uint16_t{3}, "num_0"}};
}

std::vector<std::string> Family(const std::string& before, const std::string& after) {
    std::vector<std::string> named;
    named.reserve(10);
    for (int digit = 0; digit < 10; digit++) {
        std::string one = before;
        one += static_cast<char>('0' + digit);
        one += after;
        named.push_back(std::move(one));
    }
    return named;
}

const Document::InputNumber* Stemmed(const std::vector<Document::InputNumber>& numbers,
                                     const std::string& stem) {
    const auto found = std::ranges::find(numbers, stem, &Document::InputNumber::stem);
    return found == numbers.end() ? nullptr : &*found;
}

const Document::InputSlot* Named(const Document::InputSurface& surface, const std::string& name) {
    const auto found = std::ranges::find(surface.names, name, &Document::InputSlot::name);
    return found == surface.names.end() ? nullptr : &*found;
}

}

TEST_CASE("Every named placement is an input, with what it draws and how it is driven") {
    const Document::InputSurface surface = Document::Inputs(Result(), Glyphs());

    REQUIRE(surface.names.size() == 3);
    const Document::InputSlot* ones = Named(surface, "score_this_0001");
    REQUIRE(ones != nullptr);
    CHECK(ones->clip == Document::ClipId{});
    CHECK(ones->depth == 4);
    CHECK(ones->character == std::optional<uint16_t>{uint16_t{10}});
    CHECK(ones->frames == 1);
    CHECK(ones->driven == Document::InputDriven::Texture);
    CHECK(ones->texture == std::string("num_0"));

    const Document::InputSlot* lamp = Named(surface, "clear_lamp");
    REQUIRE(lamp != nullptr);
    CHECK(lamp->frames == 3);
    CHECK(lamp->driven == Document::InputDriven::Frames);
}

TEST_CASE("A frame label is an input of the clip that holds it") {
    const Document::InputSurface surface = Document::Inputs(Result(), Glyphs());

    REQUIRE(surface.labels.size() == 1);
    CHECK(surface.labels.front().name == "loop");
    CHECK(surface.labels.front().clip == Document::ClipId{.sprite = uint16_t{11}});
    CHECK(surface.labels.front().frame == 2);
}

TEST_CASE("A placement with no name is not an input") {
    AfpAnimation::Animation animation = Result();
    animation.root.tags.push_back(AfpAnimation::Tag{Bare(9)});
    animation.root.frames.front().tag_count = 7;

    CHECK(Document::Inputs(animation, Glyphs()).names.size() == 3);
}

TEST_CASE("A name used on several depths is one input the game writes once") {
    AfpAnimation::Animation animation = Result();
    const AfpAnimation::StringId again = Document::InternString(animation, "score_this_0001");
    animation.root.tags.push_back(AfpAnimation::Tag{Places(7, 10, again)});
    animation.root.frames.front().tag_count = 7;

    const Document::InputSurface surface = Document::Inputs(animation, Glyphs());
    REQUIRE(surface.names.size() == 3);
    const Document::InputSlot* ones = Named(surface, "score_this_0001");
    REQUIRE(ones != nullptr);
    CHECK(ones->places == 2);
}

TEST_CASE("Digit places sharing a stem are one number, most significant place first") {
    const Document::InputSurface surface = Document::Inputs(Result(), Glyphs());

    const std::vector<Document::InputNumber> numbers =
        Document::Numbers(surface, Family("num_", ""));

    const Document::InputNumber* score = Stemmed(numbers, "score_this");
    REQUIRE(score != nullptr);
    CHECK(score->digit_at == 4);
    REQUIRE(score->weights.size() == 2);
    CHECK(score->weights[0] == 10);
    CHECK(score->weights[1] == 1);
    CHECK(surface.names[score->places[0]].name == "score_this_0010");
    CHECK(surface.names[score->places[1]].name == "score_this_0001");
}

TEST_CASE("The digit is found wherever it sits in the glyph name, not only at the end") {
    const Document::InputSurface surface =
        Document::Inputs(Result(), {{uint16_t{3}, "score0_color"}});

    const std::vector<Document::InputNumber> numbers =
        Document::Numbers(surface, Family("score", "_color"));

    const Document::InputNumber* score = Stemmed(numbers, "score_this");
    REQUIRE(score != nullptr);
    CHECK(score->digit_at == 5);
    CHECK(Document::Digit(surface.names[score->places[0]].texture, score->digit_at, 7) ==
          std::string("score7_color"));
}

TEST_CASE("A name with no place suffix is a number of one digit") {
    AfpAnimation::Animation animation = Result();
    const AfpAnimation::StringId alone = Document::InternString(animation, "deadpoint");
    animation.root.tags.push_back(AfpAnimation::Tag{Places(8, 10, alone)});
    animation.root.frames.front().tag_count = 7;

    const std::vector<Document::InputNumber> numbers =
        Document::Numbers(Document::Inputs(animation, Glyphs()), Family("num_", ""));

    const Document::InputNumber* dead = Stemmed(numbers, "deadpoint");
    REQUIRE(dead != nullptr);
    REQUIRE(dead->weights.size() == 1);
    CHECK(dead->weights.front() == 1);
}

TEST_CASE("An input is not a number when the package is missing one of the ten glyphs") {
    std::vector<std::string> images = Family("num_", "");
    images.erase(images.begin() + 7);

    CHECK(Document::Numbers(Document::Inputs(Result(), Glyphs()), images).empty());
}

TEST_CASE("A glyph name carries the digit it draws") {
    CHECK(Document::DigitOf("score7_color", 5) == 7);
    CHECK(Document::DigitOf("clear_lamp", 5) == 0);
    CHECK(Document::Digit("num_3", 4, 9) == std::string("num_9"));
    CHECK(Document::Digit("num_3", 9, 1).empty());
}
