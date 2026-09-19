#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/clip.h"
#include "document/frame_edit.h"
#include "document/group_sprite.h"
#include "document/tags.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

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

AfpAnimation::Placement& CreateAt(AfpAnimation::Animation& animation, uint16_t depth) {
    for (AfpAnimation::Tag& tag : animation.root.tags) {
        auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement != nullptr && placement->depth == depth) return *placement;
    }
    FAIL("no placement at the depth");
    throw;
}

std::vector<Document::Span> SpansOf(const AfpAnimation::Container& clip, uint16_t depth) {
    for (const Document::DepthRow& row : Document::DepthRows(clip)) {
        if (row.depth == depth) return row.spans;
    }
    return {};
}

const AfpAnimation::Sprite* SpriteWith(const AfpAnimation::Animation& animation, uint16_t id) {
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body);
        if (sprite != nullptr && sprite->id == id) return sprite;
    }
    return nullptr;
}

std::vector<std::string> KindsOnFrame(const AfpAnimation::Container& clip, uint32_t frame) {
    std::vector<std::string> kinds;
    const AfpAnimation::Frame& owner = clip.frames.at(frame);
    for (uint32_t i = 0; i < owner.tag_count; i++) {
        const AfpAnimation::Tag& tag = clip.tags.at(owner.first_tag + i);
        if (const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body)) {
            kinds.push_back("place " + std::to_string(placement->depth));
        } else if (const auto* remove = std::get_if<AfpAnimation::Remove>(&tag.body)) {
            kinds.push_back("remove " + std::to_string(remove->depth));
        } else if (std::holds_alternative<AfpAnimation::Sprite>(tag.body)) {
            kinds.emplace_back("sprite");
        }
    }
    return kinds;
}

Document::GroupRange Range(uint16_t first_depth, uint16_t last_depth, uint32_t first_frame,
                           uint32_t last_frame) {
    return Document::GroupRange{.clip = kRoot,
                                .first_depth = first_depth,
                                .last_depth = last_depth,
                                .first_frame = first_frame,
                                .last_frame = last_frame};
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

}

TEST_CASE("A duplicated sprite is a copy under the next free id, defined where the original is") {
    AfpAnimation::Animation animation = Clip(3);
    AfpAnimation::Sprite card{.id = 4, .container = {}};
    card.container.frames = {AfpAnimation::Frame{.first_tag = 0, .tag_count = 1},
                             AfpAnimation::Frame{.first_tag = 1, .tag_count = 0}};
    AfpAnimation::Placement inside;
    inside.depth = 1;
    inside.character = kCharacter;
    card.container.tags = {AfpAnimation::Tag{inside}};
    Document::InsertTag(animation.root, 1, AfpAnimation::Tag{card});
    animation.exports = {AfpAnimation::Export{.tag = 4, .name = 0}};

    const auto copied = Document::DuplicateSprite(animation, 4);
    INFO((copied.has_value() ? std::string() : copied.error()));
    REQUIRE(copied.has_value());
    if (!copied) return;
    CHECK(*copied == 5);
    const AfpAnimation::Sprite* original = SpriteWith(animation, 4);
    const AfpAnimation::Sprite* copy = SpriteWith(animation, 5);
    REQUIRE(original != nullptr);
    REQUIRE(copy != nullptr);
    CHECK(copy->container == original->container);
    CHECK(KindsOnFrame(animation.root, 1) == std::vector<std::string>{"sprite", "sprite"});
    CHECK(animation.exports.size() == 1);

    const AfpAnimation::Animation before = animation;
    CHECK_FALSE(Document::DuplicateSprite(animation, 99).has_value());
    CHECK_FALSE(Document::DuplicateSprite(animation, kCharacter).has_value());
    CHECK(animation == before);
}

TEST_CASE("Grouping depths moves their spans into a new sprite placed where they were") {
    AfpAnimation::Animation animation = Clip(12);
    Add(animation, 1, 0, 11);
    Add(animation, 3, 2, 6);
    Add(animation, 4, 4, 9);
    Add(animation, 6, 0, 11);
    AfpAnimation::Placement moved;
    moved.flags = 0x1;
    moved.depth = 4;
    Document::InsertTag(animation.root, 5, AfpAnimation::Tag{moved});

    const auto grouped = Document::GroupIntoSprite(animation, Range(3, 5, 2, 9));
    INFO(Error(grouped));
    REQUIRE(grouped.has_value());
    if (!grouped) return;

    CHECK(SpansOf(animation.root, 1) == std::vector<Document::Span>{{0, 11}});
    CHECK(SpansOf(animation.root, 6) == std::vector<Document::Span>{{0, 11}});
    CHECK(SpansOf(animation.root, 3) == std::vector<Document::Span>{{2, 9}});
    CHECK(SpansOf(animation.root, 4).empty());
    CHECK(CreateAt(animation, 3).character == *grouped);
    CHECK(CreateAt(animation, 3).end_frame == 10);
    CHECK(KindsOnFrame(animation.root, 0) ==
          std::vector<std::string>{"place 1", "place 6", "sprite"});
    CHECK(KindsOnFrame(animation.root, 10) == std::vector<std::string>{"remove 3"});

    const AfpAnimation::Sprite* sprite = SpriteWith(animation, *grouped);
    REQUIRE(sprite != nullptr);
    if (sprite == nullptr) return;
    const AfpAnimation::Container& inside = sprite->container;
    CHECK(inside.frames.size() == 8);
    CHECK(SpansOf(inside, 3) == std::vector<Document::Span>{{0, 4}});
    CHECK(SpansOf(inside, 4) == std::vector<Document::Span>{{2, 7}});
    CHECK(KindsOnFrame(inside, 3) == std::vector<std::string>{"place 4"});
    CHECK(KindsOnFrame(inside, 5) == std::vector<std::string>{"remove 3"});
    const auto* third = std::get_if<AfpAnimation::Placement>(&inside.tags.at(0).body);
    REQUIRE(third != nullptr);
    CHECK(third->end_frame == 5);
}

TEST_CASE("A group ends before the next span that starts on its depth") {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, 2, 1, 4);
    Add(animation, 2, 5, 8);
    const auto grouped = Document::GroupIntoSprite(animation, Range(2, 2, 1, 4));
    INFO(Error(grouped));
    REQUIRE(grouped.has_value());
    CHECK(SpansOf(animation.root, 2) == std::vector<Document::Span>{{1, 4}, {5, 8}});
    CHECK(KindsOnFrame(animation.root, 5) == std::vector<std::string>{"remove 2", "place 2"});
}

TEST_CASE("A span that runs past the grouped frames is not grouped") {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, 2, 1, 6);
    const AfpAnimation::Animation before = animation;
    CHECK_FALSE(Document::GroupIntoSprite(animation, Range(2, 2, 1, 5)).has_value());
    CHECK_FALSE(Document::GroupIntoSprite(animation, Range(2, 2, 2, 6)).has_value());
    CHECK(animation == before);
}

TEST_CASE("Frames with nothing to group are not grouped") {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, 2, 1, 6);
    CHECK_FALSE(Document::GroupIntoSprite(animation, Range(3, 4, 0, 9)).has_value());
    CHECK_FALSE(Document::GroupIntoSprite(animation, Range(4, 3, 0, 9)).has_value());
    CHECK_FALSE(Document::GroupIntoSprite(animation, Range(2, 2, 0, 10)).has_value());
}

TEST_CASE("A placement that scripts or names reach is not grouped") {
    AfpAnimation::Animation named = Clip(10);
    Add(named, 2, 1, 6);
    CreateAt(named, 2).name = AfpAnimation::StringId{0};
    CHECK_FALSE(Document::GroupIntoSprite(named, Range(2, 2, 1, 6)).has_value());

    AfpAnimation::Animation scripted = Clip(10);
    Add(scripted, 2, 1, 6);
    CreateAt(scripted, 2).clip_actions = AfpAnimation::ClipActions{};
    CHECK_FALSE(Document::GroupIntoSprite(scripted, Range(2, 2, 1, 6)).has_value());

    AfpAnimation::Animation deep = Clip(10);
    Add(deep, 2, 1, 6);
    CreateAt(deep, 2).flags |= 0x04000000;
    CHECK_FALSE(Document::GroupIntoSprite(deep, Range(2, 2, 1, 6)).has_value());
}

TEST_CASE("Frames where a clip depth is in use are not grouped") {
    AfpAnimation::Animation moved = Clip(10);
    Add(moved, 2, 1, 6);
    CreateAt(moved, 2).clip_depth = uint16_t{3};
    CHECK_FALSE(Document::GroupIntoSprite(moved, Range(2, 2, 1, 6)).has_value());

    AfpAnimation::Animation beside = Clip(10);
    Add(beside, 1, 0, 9);
    Add(beside, 2, 1, 6);
    CreateAt(beside, 1).clip_depth = uint16_t{3};
    CHECK_FALSE(Document::GroupIntoSprite(beside, Range(2, 2, 1, 6)).has_value());

    AfpAnimation::Animation earlier = Clip(10);
    Add(earlier, 1, 0, 0);
    Add(earlier, 2, 1, 6);
    CreateAt(earlier, 1).clip_depth = uint16_t{3};
    CHECK(Document::GroupIntoSprite(earlier, Range(2, 2, 1, 6)).has_value());
}

TEST_CASE("A new sprite is defined empty with the frames asked for under the next free id") {
    AfpAnimation::Animation animation = Clip(4);
    Add(animation, 2, 0, 3);
    const auto made = Document::NewSprite(animation, 12);
    REQUIRE(made.has_value());
    CHECK(*made == 0);
    const AfpAnimation::Container* sprite =
        Document::FindClip(animation, Document::ClipId{.sprite = *made});
    REQUIRE(sprite != nullptr);
    CHECK(sprite->frames.size() == 12);
    CHECK(sprite->tags.empty());
    CHECK(Document::DepthRows(animation.root).size() == 1);
    const auto second = Document::NewSprite(animation, 1);
    REQUIRE(second.has_value());
    CHECK(*second == 1);
}

TEST_CASE("A new sprite needs a frame of its own and a root frame to be defined in") {
    AfpAnimation::Animation animation = Clip(4);
    const AfpAnimation::Animation before = animation;
    CHECK_FALSE(Document::NewSprite(animation, 0).has_value());
    CHECK_FALSE(Document::NewSprite(animation, 0x10000).has_value());
    CHECK(animation == before);
    AfpAnimation::Animation empty = Clip(0);
    CHECK_FALSE(Document::NewSprite(empty, 3).has_value());
}
