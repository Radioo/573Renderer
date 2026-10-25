#include <catch2/catch_test_macros.hpp>

#include "document/animation_strings.h"
#include "document/characters.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
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
    const std::vector<Document::CharacterSummary> list = Document::Characters(Scene(), {});
    std::vector<uint16_t> ids;
    ids.reserve(list.size());
    for (const Document::CharacterSummary& one : list)
        ids.push_back(one.id);
    CHECK(ids == std::vector<uint16_t>{3, 5, 6, 8, 20});
}

TEST_CASE("A character reads as what it is") {
    const std::vector<Document::CharacterSummary> list = Document::Characters(Scene(), {});
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
    CHECK(Document::Characters(AfpAnimation::Animation{}, {}).empty());
}

TEST_CASE("A shape reads as the image it draws") {
    const std::vector<Document::CharacterSummary> list =
        Document::Characters(Scene(), {{uint16_t{8}, "bg_star"}});
    REQUIRE(WithId(list, 8) != nullptr);
    CHECK(WithId(list, 8)->label == "Shape 8: bg_star");
}

TEST_CASE("Uses count every placement that names a character, inside sprites too") {
    AfpAnimation::Animation animation = Scene();
    const auto placed = [](uint16_t depth, uint16_t character) {
        AfpAnimation::Placement placement;
        placement.depth = depth;
        placement.character = character;
        return AfpAnimation::Tag{placement};
    };
    const auto updated = [](uint16_t depth) {
        AfpAnimation::Placement placement;
        placement.depth = depth;
        return AfpAnimation::Tag{placement};
    };
    AfpAnimation::Tag gridded = placed(1, 8);
    auto& grid = std::get<AfpAnimation::Placement>(gridded.body);
    grid.extended_flags = 0;
    grid.grid_controller = AfpAnimation::GridController{.tag = 3, .first = 0, .second = 0};
    std::get<AfpAnimation::Sprite>(animation.root.tags[1].body).container.tags = {gridded,
                                                                                  updated(1)};
    std::get<AfpAnimation::Sprite>(animation.root.tags[2].body).container.tags = {placed(2, 8)};
    animation.root.tags.push_back(placed(4, 5));
    animation.root.tags.push_back(updated(4));
    animation.root.tags.push_back(placed(6, 5));
    animation.root.tags.push_back(placed(7, 20));

    const std::map<uint16_t, std::size_t> expected{
        {uint16_t{3}, 1U}, {uint16_t{5}, 2U}, {uint16_t{8}, 2U}, {uint16_t{20}, 1U}};
    CHECK(Document::CharacterUses(animation) == expected);
    CHECK(Document::CharacterUses(AfpAnimation::Animation{}).empty());
}
