#include <catch2/catch_test_macros.hpp>

#include "document/animation_strings.h"
#include "document/characters.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

AfpAnimation::Animation Scene() {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    const AfpAnimation::StringId texture = Document::InternString(animation, "bg_star");
    const AfpAnimation::StringId spinner = Document::InternString(animation, "spinner");
    animation.root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Image{.flags = 4, .id = 3, .name = texture}});
    animation.root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Sprite{.id = 5, .container = {}}});
    animation.root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Sprite{.id = 6, .container = {}}});
    animation.root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Shape{.unread_word = 0, .id = 8}});
    animation.exports.push_back(AfpAnimation::Export{.tag = 5, .name = spinner});
    const AfpAnimation::StringId movie = Document::InternString(animation, "common");
    const AfpAnimation::StringId shared = Document::InternString(animation, "button");
    animation.imports.push_back(AfpAnimation::Import{
        .movie = movie, .assets = {AfpAnimation::ImportedAsset{.tag = 20, .name = shared}}});
    return animation;
}

const Document::CharacterSummary* WithId(const std::vector<Document::CharacterSummary>& list,
                                         uint16_t id) {
    const auto found = std::ranges::find(list, id, &Document::CharacterSummary::id);
    return found == list.end() ? nullptr : &*found;
}

}

TEST_CASE("Every placeable character of an animation is listed by id") {
    const std::vector<Document::CharacterSummary> list = Document::Characters(Scene());
    std::vector<uint16_t> ids;
    ids.reserve(list.size());
    for (const Document::CharacterSummary& one : list)
        ids.push_back(one.id);
    CHECK(ids == std::vector<uint16_t>{3, 5, 6, 8, 20});
}

TEST_CASE("A character reads as what it is") {
    const std::vector<Document::CharacterSummary> list = Document::Characters(Scene());
    REQUIRE(WithId(list, 3) != nullptr);
    CHECK(WithId(list, 3)->kind == Document::CharacterKind::Image);
    CHECK(WithId(list, 3)->label == "Image 3: bg_star");
    REQUIRE(WithId(list, 5) != nullptr);
    CHECK(WithId(list, 5)->kind == Document::CharacterKind::Sprite);
    CHECK(WithId(list, 5)->label == "Sprite 5: spinner");
    REQUIRE(WithId(list, 6) != nullptr);
    CHECK(WithId(list, 6)->label == "Sprite 6");
    REQUIRE(WithId(list, 8) != nullptr);
    CHECK(WithId(list, 8)->kind == Document::CharacterKind::Shape);
    CHECK(WithId(list, 8)->label == "Shape 8");
    REQUIRE(WithId(list, 20) != nullptr);
    CHECK(WithId(list, 20)->kind == Document::CharacterKind::Imported);
    CHECK(WithId(list, 20)->label == "Imported 20: button from common");
}

TEST_CASE("An animation with nothing defined has nothing to place") {
    CHECK(Document::Characters(AfpAnimation::Animation{}).empty());
}
