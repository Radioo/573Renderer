#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/clip.h"
#include "document/key_selection.h"
#include "document/keyframes.h"
#include "document/tags.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint16_t kDepth = 3;
constexpr uint16_t kFirst = 7;
constexpr uint16_t kSecond = 9;
const Document::ClipId kRoot{};

AfpAnimation::Placement Update(std::optional<uint16_t> character, int32_t x) {
    AfpAnimation::Placement placement;
    placement.flags = kUpdateExisting | kUseMatrix;
    placement.depth = kDepth;
    placement.end_frame = 5;
    placement.character = character;
    placement.translation = std::array<int32_t, 2>{x, 0};
    return placement;
}

AfpAnimation::Animation Swapping() {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    for (int i = 0; i < 5; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    AfpAnimation::Placement create;
    create.flags = kUseMatrix;
    create.depth = kDepth;
    create.end_frame = 5;
    create.character = kFirst;
    create.translation = std::array<int32_t, 2>{0, 0};
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{create});
    Document::InsertTag(animation.root, 1, AfpAnimation::Tag{Update(std::nullopt, 20)});
    Document::InsertTag(animation.root, 2, AfpAnimation::Tag{Update(kSecond, 40)});
    Document::InsertTag(animation.root, 3, AfpAnimation::Tag{Update(std::nullopt, 60)});
    return animation;
}

const Document::Track* TrackFor(const Document::AuthoredDepth& authored,
                                const std::string& property) {
    const auto found = std::ranges::find(authored.tracks, property, &Document::Track::property);
    return found == authored.tracks.end() ? nullptr : &*found;
}

}

TEST_CASE("A depth that swaps its character is owned with a stepped character track") {
    const AfpAnimation::Animation animation = Swapping();
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    const std::string error = owned.has_value() ? std::string() : owned.error();
    INFO(error);
    REQUIRE(owned.has_value());
    if (!owned) return;
    const Document::Track* character = TrackFor(owned->authored, "Character");
    REQUIRE(character != nullptr);
    REQUIRE(character->keys.size() == 2);
    CHECK(character->keys[0].frame == 0);
    CHECK(character->keys[0].value == std::vector<int64_t>{kFirst});
    CHECK(character->keys[1].frame == 2);
    CHECK(character->keys[1].value == std::vector<int64_t>{kSecond});
    CHECK_FALSE(owned->baked.create.character.has_value());

    AfpAnimation::Animation written = animation;
    REQUIRE(Document::WriteAuthored(written, owned->authored, owned->baked).has_value());
    CHECK(written == animation);
}

TEST_CASE("A depth that keeps its character has no character track") {
    AfpAnimation::Animation animation = Swapping();
    auto& swap = std::get<AfpAnimation::Placement>(
        animation.root.tags[animation.root.frames[2].first_tag].body);
    swap.character.reset();
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    if (!owned) return;
    CHECK(TrackFor(owned->authored, "Character") == nullptr);
    CHECK(owned->baked.create.character == kFirst);
    AfpAnimation::Animation written = animation;
    REQUIRE(Document::WriteAuthored(written, owned->authored, owned->baked).has_value());
    CHECK(written == animation);
}

TEST_CASE("A character only holds from one keyframe to the next") {
    const AfpAnimation::Animation animation = Swapping();
    auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    if (!owned) return;
    CHECK_FALSE(Document::SetKeysEase(owned->authored,
                                      {Document::KeyRef{.property = "Character", .frame = 0}},
                                      Document::Ease::Linear, {})
                    .has_value());

    Document::AuthoredDepth eased = owned->authored;
    const auto character =
        std::ranges::find(eased.tracks, std::string("Character"), &Document::Track::property);
    REQUIRE(character != eased.tracks.end());
    character->keys[0].ease = Document::Ease::Linear;
    AfpAnimation::Animation written = animation;
    CHECK_FALSE(Document::WriteAuthored(written, eased, owned->baked).has_value());
}

TEST_CASE("Moving a character key moves the swap") {
    const AfpAnimation::Animation animation = Swapping();
    auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    if (!owned) return;
    REQUIRE(Document::ShiftKeys(owned->authored,
                                {Document::KeyRef{.property = "Character", .frame = 2}}, 1)
                .has_value());
    AfpAnimation::Animation written = animation;
    REQUIRE(Document::WriteAuthored(written, owned->authored, owned->baked).has_value());
    const auto& moved =
        std::get<AfpAnimation::Placement>(written.root.tags[written.root.frames[3].first_tag].body);
    CHECK(moved.character == kSecond);
    const auto& before =
        std::get<AfpAnimation::Placement>(written.root.tags[written.root.frames[2].first_tag].body);
    CHECK_FALSE(before.character.has_value());
}
