#include <catch2/catch_test_macros.hpp>

#include "document/placement_effect.h"
#include "document/tags.h"
#include "formats/afp_animation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kUseColour = 0x8;
constexpr uint32_t kThreeD = 0x04000000;
constexpr uint16_t kDepth = 2;

AfpAnimation::Container Frames(std::size_t count) {
    AfpAnimation::Container clip;
    for (std::size_t i = 0; i < count; i++)
        clip.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    return clip;
}

AfpAnimation::Placement Placed(uint32_t flags) {
    AfpAnimation::Placement placement;
    placement.flags = flags;
    placement.depth = kDepth;
    placement.end_frame = 9;
    return placement;
}

std::optional<Document::AppliedState> StateOn(const AfpAnimation::Container& clip, uint32_t frame) {
    for (const auto& [on, state] : Document::ReplayDepth(clip, kDepth, 0, 8)) {
        if (on == frame) return state;
    }
    return std::nullopt;
}

}

TEST_CASE("A created object starts from the identity and takes what it carries") {
    AfpAnimation::Container clip = Frames(2);
    AfpAnimation::Placement create = Placed(kUseMatrix | kUseColour);
    create.character = uint16_t{1};
    create.scale = std::array<int32_t, 2>{2048, 512};
    create.add_colour = std::array<int16_t, 4>{255, 0, 0, 0};
    Document::InsertTag(clip, 0, AfpAnimation::Tag{create});

    const auto state = StateOn(clip, 0);
    REQUIRE(state.has_value());
    if (!state) return;
    CHECK(state->matrix == std::array<double, 6>{2.0, 0.0, 0.0, 0.5, 0.0, 0.0});
    CHECK(state->multiply == std::array<double, 4>{1.0, 1.0, 1.0, 1.0});
    CHECK(state->add == std::array<double, 4>{1.0, 0.0, 0.0, 0.0});
}

TEST_CASE("A matrix update resets the parts of the matrix it does not carry") {
    AfpAnimation::Container clip = Frames(3);
    AfpAnimation::Placement create = Placed(kUseMatrix);
    create.scale = std::array<int32_t, 2>{2048, 2048};
    create.translation = std::array<int32_t, 2>{10, 20};
    Document::InsertTag(clip, 0, AfpAnimation::Tag{create});
    AfpAnimation::Placement moved = Placed(kUpdateExisting | kUseMatrix);
    moved.translation = std::array<int32_t, 2>{30, 40};
    Document::InsertTag(clip, 1, AfpAnimation::Tag{moved});

    const auto state = StateOn(clip, 1);
    REQUIRE(state.has_value());
    if (!state) return;
    CHECK(state->matrix == std::array<double, 6>{1.0, 0.0, 0.0, 1.0, 30.0, 40.0});
}

TEST_CASE("An update without the matrix bit leaves the matrix alone") {
    AfpAnimation::Container clip = Frames(3);
    AfpAnimation::Placement create = Placed(kUseMatrix);
    create.scale = std::array<int32_t, 2>{2048, 2048};
    Document::InsertTag(clip, 0, AfpAnimation::Tag{create});
    AfpAnimation::Placement tinted = Placed(kUpdateExisting | kUseColour);
    tinted.multiply_colour = std::array<int16_t, 4>{0, 255, 255, 255};
    Document::InsertTag(clip, 1, AfpAnimation::Tag{tinted});

    const auto state = StateOn(clip, 1);
    REQUIRE(state.has_value());
    if (!state) return;
    CHECK(state->matrix[0] == 2.0);
    CHECK(state->multiply == std::array<double, 4>{0.0, 1.0, 1.0, 1.0});
}

TEST_CASE("A colour update resets the colour it does not carry") {
    AfpAnimation::Container clip = Frames(3);
    AfpAnimation::Placement create = Placed(kUseMatrix | kUseColour);
    create.add_colour = std::array<int16_t, 4>{255, 255, 0, 0};
    Document::InsertTag(clip, 0, AfpAnimation::Tag{create});
    Document::InsertTag(clip, 1, AfpAnimation::Tag{Placed(kUpdateExisting | kUseColour)});

    const auto state = StateOn(clip, 1);
    REQUIRE(state.has_value());
    if (!state) return;
    CHECK(state->add == std::array<double, 4>{0.0, 0.0, 0.0, 0.0});
}

TEST_CASE("A short scale is read over the long one, as the game parses it") {
    AfpAnimation::Container clip = Frames(1);
    AfpAnimation::Placement create = Placed(kUseMatrix);
    create.scale = std::array<int32_t, 2>{2048, 2048};
    create.short_scale = std::array<int16_t, 2>{16384, 16384};
    Document::InsertTag(clip, 0, AfpAnimation::Tag{create});

    const auto state = StateOn(clip, 0);
    REQUIRE(state.has_value());
    if (!state) return;
    CHECK(state->matrix[0] == 0.5);
    CHECK(state->matrix[3] == 0.5);
}

TEST_CASE("A packed colour is read over the unpacked one, byte by byte") {
    AfpAnimation::Container clip = Frames(1);
    AfpAnimation::Placement create = Placed(kUseMatrix | kUseColour);
    create.multiply_colour = std::array<int16_t, 4>{0, 0, 0, 0};
    create.packed_multiply_colour = uint32_t{0xFF0000FF};
    Document::InsertTag(clip, 0, AfpAnimation::Tag{create});

    const auto state = StateOn(clip, 0);
    REQUIRE(state.has_value());
    if (!state) return;
    CHECK(state->multiply == std::array<double, 4>{1.0, 0.0, 0.0, 1.0});
}

TEST_CASE("A 3D update resets only the translation") {
    AfpAnimation::Container clip = Frames(3);
    AfpAnimation::Placement create = Placed(kUseMatrix | kThreeD);
    create.translation = std::array<int32_t, 2>{10, 20};
    create.scale = std::array<int32_t, 2>{2048, 2048};
    Document::InsertTag(clip, 0, AfpAnimation::Tag{create});
    Document::InsertTag(clip, 1, AfpAnimation::Tag{Placed(kUpdateExisting | kUseMatrix | kThreeD)});

    const auto state = StateOn(clip, 1);
    REQUIRE(state.has_value());
    if (!state) return;
    CHECK(state->matrix[4] == 0.0);
    CHECK(state->matrix[5] == 0.0);
    CHECK(state->matrix[0] == 1.0);
}

TEST_CASE("A depth is replayed only while it is on the stage") {
    AfpAnimation::Container clip = Frames(4);
    Document::InsertTag(clip, 1, AfpAnimation::Tag{Placed(kUseMatrix)});
    Document::InsertTag(clip, 3,
                        AfpAnimation::Tag{AfpAnimation::Remove{.unread_word = 0, .depth = kDepth}});

    std::vector<uint32_t> frames;
    for (const auto& [frame, state] : Document::ReplayDepth(clip, kDepth, 0, 3))
        frames.push_back(frame);
    CHECK(frames == std::vector<uint32_t>{1, 2});
}

TEST_CASE("An update to a depth nothing placed is ignored, as the game ignores it") {
    AfpAnimation::Container clip = Frames(2);
    AfpAnimation::Placement orphan = Placed(kUpdateExisting | kUseMatrix);
    orphan.translation = std::array<int32_t, 2>{5, 5};
    Document::InsertTag(clip, 0, AfpAnimation::Tag{orphan});
    CHECK(Document::ReplayDepth(clip, kDepth, 0, 1).empty());
}
