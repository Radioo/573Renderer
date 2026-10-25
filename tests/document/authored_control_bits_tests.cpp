#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/clip.h"
#include "document/keyframe_edit.h"
#include "document/tags.h"
#include "formats/afp_animation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kUseColour = 0x8;
constexpr uint32_t kThreeD = 0x04000000;
constexpr uint32_t kHiddenInSomeModes = 0x200000;
constexpr uint16_t kDepth = 3;
const Document::ClipId kRoot{};

AfpAnimation::Animation Clip(std::size_t frames) {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    for (std::size_t i = 0; i < frames; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    return animation;
}

AfpAnimation::Placement Create() {
    AfpAnimation::Placement placement;
    placement.flags = kUseMatrix;
    placement.depth = kDepth;
    placement.end_frame = 5;
    placement.character = uint16_t{7};
    placement.translation = std::array<int32_t, 2>{0, 0};
    return placement;
}

AfpAnimation::Placement Update(uint32_t controls) {
    AfpAnimation::Placement placement;
    placement.flags = kUpdateExisting | controls;
    placement.depth = kDepth;
    placement.end_frame = 5;
    return placement;
}

AfpAnimation::Placement Moved(int32_t x) {
    AfpAnimation::Placement placement = Update(kUseMatrix);
    placement.translation = std::array<int32_t, 2>{x, 0};
    return placement;
}

AfpAnimation::Placement Tinted(int16_t red) {
    AfpAnimation::Placement placement = Update(kUseColour);
    placement.multiply_colour = std::array<int16_t, 4>{red, 255, 255, 255};
    return placement;
}

AfpAnimation::Animation Mixed() {
    AfpAnimation::Animation animation = Clip(6);
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{Create()});
    Document::InsertTag(animation.root, 1, AfpAnimation::Tag{Moved(10)});
    Document::InsertTag(animation.root, 2, AfpAnimation::Tag{Tinted(100)});
    AfpAnimation::Placement both = Moved(30);
    both.flags |= kUseColour;
    both.multiply_colour = std::array<int16_t, 4>{50, 255, 255, 255};
    Document::InsertTag(animation.root, 3, AfpAnimation::Tag{both});
    Document::InsertTag(animation.root, 4, AfpAnimation::Tag{Update(kUseColour)});
    return animation;
}

const AfpAnimation::Placement* PlacementOn(const AfpAnimation::Animation& animation,
                                           uint32_t frame) {
    const AfpAnimation::Frame& owner = animation.root.frames[frame];
    for (uint32_t i = 0; i < owner.tag_count; i++) {
        const auto* placement =
            std::get_if<AfpAnimation::Placement>(&animation.root.tags[owner.first_tag + i].body);
        if (placement != nullptr && placement->depth == kDepth) return placement;
    }
    return nullptr;
}

void Rewrite(AfpAnimation::Animation& animation, const Document::AuthoredDepth& authored) {
    const auto baked = Document::BakedFor(animation, authored);
    REQUIRE(baked.has_value());
    const auto written = Document::WriteAuthored(animation, authored, *baked);
    REQUIRE(written.has_value());
}

}

TEST_CASE("A span whose updates use different control bits is owned and comes back exactly") {
    AfpAnimation::Animation animation = Mixed();
    const AfpAnimation::Animation before = animation;
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    Rewrite(animation, owned->authored);
    CHECK(animation == before);
}

TEST_CASE("An update that sets a control bit without the field keeps the bit") {
    AfpAnimation::Animation animation = Mixed();
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    Rewrite(animation, owned->authored);
    const AfpAnimation::Placement* blank = PlacementOn(animation, 4);
    REQUIRE(blank != nullptr);
    CHECK(blank->flags == (kUpdateExisting | kUseColour));
}

TEST_CASE("A colour keyed on a frame that only moved gets the bit that applies it") {
    AfpAnimation::Animation animation = Mixed();
    auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    REQUIRE(Document::AddKeyAt(owned->authored, "Multiply colour", 1).has_value());
    Rewrite(animation, owned->authored);

    const AfpAnimation::Placement* moved = PlacementOn(animation, 1);
    REQUIRE(moved != nullptr);
    CHECK(moved->multiply_colour.has_value());
    CHECK((moved->flags & kUseColour) != 0);
    CHECK((moved->flags & kUseMatrix) != 0);
}

TEST_CASE("A matrix keyed on a frame that only tinted gets the bit that applies it") {
    AfpAnimation::Animation animation = Mixed();
    auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    REQUIRE(Document::AddKeyAt(owned->authored, "Translation", 2).has_value());
    Rewrite(animation, owned->authored);

    const AfpAnimation::Placement* tinted = PlacementOn(animation, 2);
    REQUIRE(tinted != nullptr);
    CHECK(tinted->translation.has_value());
    CHECK((tinted->flags & kUseMatrix) != 0);
    CHECK((tinted->flags & kUseColour) != 0);
}

TEST_CASE("A colour keyed on the first frame gets the bit on the create") {
    AfpAnimation::Animation animation = Mixed();
    auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    REQUIRE(Document::AddKeyAt(owned->authored, "Multiply colour", 0).has_value());
    Rewrite(animation, owned->authored);

    const AfpAnimation::Placement* create = PlacementOn(animation, 0);
    REQUIRE(create != nullptr);
    CHECK((create->flags & kUpdateExisting) == 0);
    CHECK(create->multiply_colour.has_value());
    CHECK((create->flags & kUseColour) != 0);
}

TEST_CASE("A field the game would not apply is not owned") {
    AfpAnimation::Animation animation = Clip(3);
    AfpAnimation::Placement create = Create();
    create.multiply_colour = std::array<int16_t, 4>{1, 2, 3, 4};
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{create});
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE_FALSE(owned.has_value());
    CHECK(owned.error().find("colour") != std::string::npos);
}

TEST_CASE("An update moving without the bit that applies it is not owned") {
    AfpAnimation::Animation animation = Clip(3);
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{Create()});
    AfpAnimation::Placement moved = Moved(5);
    moved.flags = kUpdateExisting;
    Document::InsertTag(animation.root, 1, AfpAnimation::Tag{moved});
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE_FALSE(owned.has_value());
    CHECK(owned.error().find("matrix") != std::string::npos);
}

TEST_CASE("Updates that differ in anything but the control bits are still not owned") {
    AfpAnimation::Animation animation = Clip(4);
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{Create()});
    Document::InsertTag(animation.root, 1, AfpAnimation::Tag{Moved(1)});
    AfpAnimation::Placement odd = Moved(2);
    odd.flags |= kHiddenInSomeModes;
    Document::InsertTag(animation.root, 2, AfpAnimation::Tag{odd});
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE_FALSE(owned.has_value());
    CHECK(owned.error().find("flags of its own") != std::string::npos);
}

TEST_CASE("A span that switches between 2D and 3D is not owned") {
    AfpAnimation::Animation animation = Clip(4);
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{Create()});
    AfpAnimation::Placement flat = Moved(2);
    flat.flags |= kThreeD;
    Document::InsertTag(animation.root, 1, AfpAnimation::Tag{flat});
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE_FALSE(owned.has_value());
    CHECK(owned.error().find("between 2D and 3D") != std::string::npos);
}
