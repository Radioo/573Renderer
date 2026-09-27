#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/clip.h"
#include "document/frame_edit.h"
#include "document/group_sprite.h"
#include "document/span_edit.h"
#include "document/tags.h"
#include "formats/afp_animation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>

namespace {

constexpr uint16_t kCharacter = 7;
const Document::ClipId kRoot{};

AfpAnimation::Animation Clip(std::size_t frames) {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    for (std::size_t i = 0; i < frames; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    return animation;
}

void Add(AfpAnimation::Animation& animation, uint16_t depth, uint32_t first, uint32_t last) {
    REQUIRE(Document::AddDepth(animation, kRoot, depth, kCharacter, first, last).has_value());
}

AfpAnimation::Animation Layered() {
    AfpAnimation::Animation animation = Clip(12);
    Add(animation, 1, 0, 11);
    Add(animation, 3, 2, 6);
    Add(animation, 4, 4, 9);
    Add(animation, 6, 0, 11);
    AfpAnimation::Placement moved;
    moved.flags = 0x1;
    moved.depth = 4;
    moved.end_frame = 10;
    moved.translation = std::array<int32_t, 2>{5, 5};
    Document::InsertTag(animation.root, 5, AfpAnimation::Tag{moved});
    return animation;
}

uint16_t Group(AfpAnimation::Animation& animation, uint16_t first_depth, uint16_t last_depth,
               uint32_t first_frame, uint32_t last_frame) {
    const auto grouped =
        Document::GroupIntoSprite(animation, Document::GroupRange{.clip = kRoot,
                                                                  .first_depth = first_depth,
                                                                  .last_depth = last_depth,
                                                                  .first_frame = first_frame,
                                                                  .last_frame = last_frame});
    const std::string error = grouped.has_value() ? std::string() : grouped.error();
    INFO(error);
    REQUIRE(grouped.has_value());
    return grouped.value_or(0);
}

AfpAnimation::Placement& SpritePlacement(AfpAnimation::Animation& animation, uint16_t sprite) {
    for (AfpAnimation::Tag& tag : animation.root.tags) {
        auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement != nullptr && placement->character == sprite) return *placement;
    }
    FAIL("the sprite is not placed");
    throw;
}

AfpAnimation::Container& Inside(AfpAnimation::Animation& animation, uint16_t sprite) {
    for (AfpAnimation::Tag& tag : animation.root.tags) {
        auto* found = std::get_if<AfpAnimation::Sprite>(&tag.body);
        if (found != nullptr && found->id == sprite) return found->container;
    }
    FAIL("the sprite is not defined");
    throw;
}

bool Defined(const AfpAnimation::Animation& animation, uint16_t sprite) {
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* found = std::get_if<AfpAnimation::Sprite>(&tag.body);
        if (found != nullptr && found->id == sprite) return true;
    }
    return false;
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

}

TEST_CASE("Ungrouping a grouped sprite gives back the clip it came from") {
    const AfpAnimation::Animation original = Layered();
    AfpAnimation::Animation animation = original;
    Group(animation, 3, 5, 2, 9);
    const auto ungrouped = Document::UngroupSprite(animation, kRoot, 3, 5);
    INFO(Error(ungrouped));
    REQUIRE(ungrouped.has_value());
    CHECK(animation == original);
}

TEST_CASE("Ungrouping from frame 0 to the last frame gives back the clip") {
    AfpAnimation::Animation original = Clip(6);
    Add(original, 2, 0, 5);
    Add(original, 3, 1, 3);
    AfpAnimation::Animation animation = original;
    Group(animation, 2, 3, 0, 5);
    const auto ungrouped = Document::UngroupSprite(animation, kRoot, 2, 0);
    INFO(Error(ungrouped));
    REQUIRE(ungrouped.has_value());
    CHECK(animation == original);
}

TEST_CASE("Only a sprite placed once and left as it is can be ungrouped") {
    AfpAnimation::Animation moved = Layered();
    Group(moved, 3, 5, 2, 9);
    AfpAnimation::Placement update;
    update.flags = 0x1;
    update.depth = 3;
    Document::InsertTag(moved.root, 4, AfpAnimation::Tag{update});
    CHECK_FALSE(Document::UngroupSprite(moved, kRoot, 3, 5).has_value());

    AfpAnimation::Animation turned = Layered();
    const uint16_t turned_sprite = Group(turned, 3, 5, 2, 9);
    SpritePlacement(turned, turned_sprite).flags = 0x4;
    SpritePlacement(turned, turned_sprite).translation = std::array<int32_t, 2>{0, 0};
    const AfpAnimation::Animation before = turned;
    CHECK_FALSE(Document::UngroupSprite(turned, kRoot, 3, 5).has_value());
    CHECK(turned == before);

    AfpAnimation::Animation longer = Layered();
    const uint16_t longer_sprite = Group(longer, 3, 5, 2, 9);
    Inside(longer, longer_sprite)
        .frames.push_back(AfpAnimation::Frame{
            .first_tag = static_cast<uint32_t>(Inside(longer, longer_sprite).tags.size()),
            .tag_count = 0});
    CHECK_FALSE(Document::UngroupSprite(longer, kRoot, 3, 5).has_value());

    AfpAnimation::Animation plain = Layered();
    CHECK_FALSE(Document::UngroupSprite(plain, kRoot, 1, 5).has_value());
    CHECK_FALSE(Document::UngroupSprite(plain, kRoot, 2, 0).has_value());
}

TEST_CASE("A sprite holding more than placements is not ungrouped") {
    AfpAnimation::Animation labelled = Layered();
    const uint16_t sprite = Group(labelled, 3, 5, 2, 9);
    Inside(labelled, sprite).labels.push_back(AfpAnimation::Label{.frame = 0, .name = 0});
    CHECK_FALSE(Document::UngroupSprite(labelled, kRoot, 3, 5).has_value());

    AfpAnimation::Animation acting = Layered();
    const uint16_t acting_sprite = Group(acting, 3, 5, 2, 9);
    Document::InsertTag(Inside(acting, acting_sprite), 1,
                        AfpAnimation::Tag{AfpAnimation::Action{}});
    CHECK_FALSE(Document::UngroupSprite(acting, kRoot, 3, 5).has_value());
}

TEST_CASE("Ungrouping refuses to put depths between other depths of the clip") {
    AfpAnimation::Animation animation = Layered();
    Group(animation, 3, 5, 2, 9);
    Add(animation, 4, 0, 11);
    const AfpAnimation::Animation before = animation;
    const auto ungrouped = Document::UngroupSprite(animation, kRoot, 3, 5);
    REQUIRE_FALSE(ungrouped.has_value());
    CHECK(ungrouped.error().find("depth 4") != std::string::npos);
    CHECK(animation == before);
}

TEST_CASE("A sprite placed elsewhere or exported keeps its definition") {
    AfpAnimation::Animation twice = Layered();
    const uint16_t sprite = Group(twice, 3, 5, 2, 9);
    REQUIRE(Document::DuplicateSpan(twice, kRoot, 3, 2, 20).has_value());
    REQUIRE(Document::UngroupSprite(twice, kRoot, 3, 5).has_value());
    CHECK(Defined(twice, sprite));

    AfpAnimation::Animation exported = Layered();
    const uint16_t exported_sprite = Group(exported, 3, 5, 2, 9);
    exported.exports.push_back(AfpAnimation::Export{.tag = exported_sprite, .name = 0});
    REQUIRE(Document::UngroupSprite(exported, kRoot, 3, 5).has_value());
    CHECK(Defined(exported, exported_sprite));

    AfpAnimation::Animation once = Layered();
    const uint16_t once_sprite = Group(once, 3, 5, 2, 9);
    REQUIRE(Document::UngroupSprite(once, kRoot, 3, 5).has_value());
    CHECK_FALSE(Defined(once, once_sprite));
}
