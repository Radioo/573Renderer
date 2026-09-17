#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/clip.h"
#include "document/placement_effect.h"
#include "document/stage_move.h"
#include "document/tags.h"
#include "formats/afp_animation.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kUseColour = 0x8;
constexpr uint32_t kThreeD = 0x04000000;
constexpr uint16_t kDepth = 3;
constexpr uint32_t kFrames = 4;
const Document::ClipId kRoot{};

AfpAnimation::Animation Placed(uint32_t mode) {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    for (uint32_t i = 0; i < kFrames; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    AfpAnimation::Placement create;
    create.flags = kUseMatrix | mode;
    create.depth = kDepth;
    create.end_frame = kFrames;
    create.character = uint16_t{7};
    create.scale = std::array<int32_t, 2>{2048, 1024};
    create.translation = std::array<int32_t, 2>{100, 40};
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{create});
    return animation;
}

std::optional<Document::AppliedState> StateOn(const AfpAnimation::Animation& animation,
                                              uint32_t frame) {
    const auto shown = Document::ReplayDepth(animation.root, kDepth, frame, frame);
    if (shown.empty()) return std::nullopt;
    return shown.back().second;
}

}

TEST_CASE("Moving a depth adds the offset to the translation it is placed with, in twentieths") {
    AfpAnimation::Animation animation = Placed(0);
    REQUIRE(Document::MoveBakedDepth(animation, kRoot, kDepth, 2, {.x = 3, .y = -1.5}).has_value());
    const auto state = StateOn(animation, 2);
    REQUIRE(state.has_value());
    if (!state) return;
    CHECK(state->matrix == std::array<double, 6>{2.0, 0.0, 0.0, 1.0, 160.0, 10.0});
}

TEST_CASE("Moving where only the colour changes keeps the matrix it was showing") {
    AfpAnimation::Animation animation = Placed(0);
    AfpAnimation::Placement tinted;
    tinted.flags = kUpdateExisting | kUseColour;
    tinted.depth = kDepth;
    tinted.end_frame = kFrames;
    tinted.multiply_colour = std::array<int16_t, 4>{255, 0, 0, 255};
    Document::InsertTag(animation.root, 1, AfpAnimation::Tag{tinted});

    REQUIRE(Document::MoveBakedDepth(animation, kRoot, kDepth, 2, {.x = 1, .y = 1}).has_value());
    const auto before = StateOn(animation, 0);
    const auto after = StateOn(animation, 3);
    REQUIRE(before.has_value());
    REQUIRE(after.has_value());
    if (!before || !after) return;
    CHECK(before->matrix == std::array<double, 6>{2.0, 0.0, 0.0, 1.0, 100.0, 40.0});
    CHECK(after->matrix == std::array<double, 6>{2.0, 0.0, 0.0, 1.0, 120.0, 60.0});
    CHECK(after->multiply == std::array<double, 4>{1.0, 0.0, 0.0, 1.0});
}

TEST_CASE("A depth that is not there or is in 3D is not moved") {
    AfpAnimation::Animation flat = Placed(0);
    CHECK_FALSE(Document::MoveBakedDepth(flat, kRoot, 9, 0, {.x = 1, .y = 1}).has_value());
    CHECK_FALSE(Document::MoveBakedDepth(flat, Document::ClipId{.sprite = uint16_t{4}}, kDepth, 0,
                                         {.x = 1, .y = 1})
                    .has_value());
    AfpAnimation::Animation deep = Placed(kThreeD);
    CHECK_FALSE(Document::MoveBakedDepth(deep, kRoot, kDepth, 0, {.x = 1, .y = 1}).has_value());
}

TEST_CASE("Moving an owned depth keys its translation on that frame") {
    const AfpAnimation::Animation animation = Placed(0);
    auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    if (!owned) return;
    REQUIRE(
        Document::MoveOwnedDepth(owned->authored, owned->baked, 2, {.x = -5, .y = 0}).has_value());

    AfpAnimation::Animation written = animation;
    const auto result = Document::WriteAuthored(written, owned->authored, owned->baked);
    const std::string error = result.has_value() ? std::string() : result.error();
    INFO(error);
    REQUIRE(result.has_value());
    const auto moved = StateOn(written, 2);
    const auto held = StateOn(written, 0);
    REQUIRE(moved.has_value());
    REQUIRE(held.has_value());
    if (!moved || !held) return;
    CHECK(moved->matrix[4] == 0.0);
    CHECK(held->matrix[4] == 100.0);
}
