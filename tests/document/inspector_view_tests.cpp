#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "document/authored.h"
#include "document/inspector_view.h"
#include "document/keyframe_edit.h"
#include "document/tags.h"
#include "formats/afp_animation.h"

#include "sample_package.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kUseColour = 0x8;
constexpr uint16_t kDepth = 2;
constexpr double kClose = 1e-6;

AfpAnimation::Placement Placed(uint32_t flags) {
    AfpAnimation::Placement placement;
    placement.flags = flags;
    placement.depth = kDepth;
    placement.end_frame = 9;
    return placement;
}

AfpAnimation::Animation Scene() {
    AfpAnimation::Animation animation;
    for (int i = 0; i < 4; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    AfpAnimation::Placement create = Placed(kUseMatrix | kUseColour);
    create.character = uint16_t{1};
    create.translation = std::array<int32_t, 2>{200, -100};
    create.scale = std::array<int32_t, 2>{2048, 1024};
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{create});
    AfpAnimation::Placement tinted = Placed(kUpdateExisting | kUseColour);
    tinted.multiply_colour = std::array<int16_t, 4>{255, 0, 0, 255};
    Document::InsertTag(animation.root, 2, AfpAnimation::Tag{tinted});
    return animation;
}

const Document::ViewRow& Row(const Document::PlacementView& view, std::string_view label) {
    for (const std::vector<Document::ViewRow>* rows : {&view.transform, &view.colours}) {
        const auto found = std::ranges::find(*rows, label, &Document::ViewRow::label);
        if (found != rows->end()) return *found;
    }
    FAIL("no row " << label);
    static const Document::ViewRow none;
    return none;
}

Document::PlacementView ViewOn(const AfpAnimation::Animation& animation, uint32_t frame,
                               const Document::AuthoredDepth* owned = nullptr) {
    const auto view = Document::ViewPlacement(animation.root, kDepth, frame, owned);
    REQUIRE(view.has_value());
    return *view;
}

void CheckValues(const Document::ViewRow& row, const std::vector<double>& wanted) {
    REQUIRE(row.values.size() == wanted.size());
    for (std::size_t i = 0; i < wanted.size(); i++)
        CHECK_THAT(row.values[i], WithinAbs(wanted[i], kClose));
}

const AfpAnimation::Placement& Live(const AfpAnimation::Animation& animation, std::size_t tag) {
    const auto* placement = std::get_if<AfpAnimation::Placement>(&animation.root.tags.at(tag).body);
    REQUIRE(placement != nullptr);
    return *placement;
}

}

TEST_CASE(
    "The inspector view shows pixels, percent and degrees and the frame each group was set on") {
    const AfpAnimation::Animation animation = Scene();
    const Document::PlacementView first = ViewOn(animation, 1);
    CheckValues(Row(first, "Position"), {10, -5});
    CheckValues(Row(first, "Scale"), {200, 100});
    CheckValues(Row(first, "Rotation"), {0});
    CheckValues(Row(first, "Skew"), {0});
    CheckValues(Row(first, "Multiply"), {255, 255, 255, 255});
    CHECK(Row(first, "Position").set_on == 0);
    CHECK(Row(first, "Multiply").set_on == 0);
    CHECK(Row(first, "Position").keying == Document::Keying::Baked);

    const Document::PlacementView third = ViewOn(animation, 2);
    CheckValues(Row(third, "Multiply"), {255, 0, 0, 255});
    CHECK(Row(third, "Multiply").set_on == 2);
    CHECK(Row(third, "Add").set_on == 2);
    CHECK(Row(third, "Position").set_on == 0);
    CHECK_FALSE(Document::ViewPlacement(animation.root, 7, 1, nullptr).has_value());
}

TEST_CASE("A matrix update in the view resets the scale it leaves out, as the game does") {
    AfpAnimation::Animation animation = Scene();
    AfpAnimation::Placement moved = Placed(kUpdateExisting | kUseMatrix);
    moved.translation = std::array<int32_t, 2>{400, 0};
    Document::InsertTag(animation.root, 1, AfpAnimation::Tag{moved});
    const Document::PlacementView view = ViewOn(animation, 1);
    CheckValues(Row(view, "Position"), {20, 0});
    CheckValues(Row(view, "Scale"), {100, 100});
    CHECK(Row(view, "Scale").set_on == 1);
}

TEST_CASE("Setting rotation, skew or scale in the view rewrites the live placement's matrix") {
    AfpAnimation::Animation animation = Scene();
    REQUIRE(Document::SetViewedBaked(animation, {}, kDepth, 1, "Rotation", {30}).has_value());
    const Document::PlacementView turned = ViewOn(animation, 1);
    CHECK_THAT(Row(turned, "Rotation").values.at(0), WithinAbs(30, 0.1));
    CHECK_THAT(Row(turned, "Scale").values.at(0), WithinAbs(200, 0.1));
    CHECK_THAT(Row(turned, "Scale").values.at(1), WithinAbs(100, 0.1));
    CheckValues(Row(turned, "Position"), {10, -5});

    REQUIRE(Document::SetViewedBaked(animation, {}, kDepth, 1, "Scale", {50, 50}).has_value());
    const Document::PlacementView scaled = ViewOn(animation, 1);
    CHECK_THAT(Row(scaled, "Scale").values.at(0), WithinAbs(50, 0.1));
    CHECK_THAT(Row(scaled, "Rotation").values.at(0), WithinAbs(30, 0.1));

    REQUIRE(Document::SetViewedBaked(animation, {}, kDepth, 1, "Skew", {10}).has_value());
    CHECK_THAT(Row(ViewOn(animation, 1), "Skew").values.at(0), WithinAbs(10, 0.1));
    CHECK_FALSE(Document::SetViewedBaked(animation, {}, kDepth, 1, "Skew", {10, 1}).has_value());
    CHECK_FALSE(Document::SetViewedBaked(animation, {}, kDepth, 1, "Nothing", {1}).has_value());
}

TEST_CASE("Setting a position in the view moves the depth by the difference") {
    AfpAnimation::Animation animation = Scene();
    REQUIRE(Document::SetViewedBaked(animation, {}, kDepth, 0, "Position", {15, 5}).has_value());
    CheckValues(Row(ViewOn(animation, 0), "Position"), {15, 5});
    CHECK(Live(animation, 0).translation == std::array<int32_t, 2>{300, 100});
}

TEST_CASE("Setting a colour on a placement without the colour bit keeps the other colour") {
    AfpAnimation::Animation animation = Scene();
    AfpAnimation::Placement moved = Placed(kUpdateExisting | kUseMatrix);
    moved.translation = std::array<int32_t, 2>{200, -100};
    moved.scale = std::array<int32_t, 2>{2048, 1024};
    Document::InsertTag(animation.root, 3, AfpAnimation::Tag{moved});
    const std::size_t tag = animation.root.frames[3].first_tag;
    REQUIRE((Live(animation, tag).flags & kUseColour) == 0);

    REQUIRE(Document::SetViewedBaked(animation, {}, kDepth, 3, "Add", {0, 0, 64, 0}).has_value());
    const Document::PlacementView view = ViewOn(animation, 3);
    CheckValues(Row(view, "Add"), {0, 0, 64, 0});
    CheckValues(Row(view, "Multiply"), {255, 0, 0, 255});
    CHECK(Row(view, "Add").set_on == 3);
    CHECK((Live(animation, tag).flags & kUseColour) != 0);
}

TEST_CASE("The view of an owned depth says which values are animated and keyed on the frame") {
    AfpAnimation::Animation animation = SamplePackage::SampleAnimation();
    animation.root.labels = {};
    AfpAnimation::Placement create = Placed(kUseMatrix);
    create.depth = kDepth;
    create.end_frame = 3;
    create.character = uint16_t{7};
    create.translation = std::array<int32_t, 2>{0, 0};
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{create});
    auto owned = Document::OwnDepth(animation, {}, "afp/intro", kDepth, 0);
    REQUIRE(owned.has_value());
    Document::AuthoredDepth authored = owned->authored;

    const Document::PlacementView before = ViewOn(animation, 0, &authored);
    CHECK(Row(before, "Position").keying == Document::Keying::KeyedHere);
    CHECK(Row(before, "Rotation").keying == Document::Keying::NotAnimated);

    REQUIRE(Document::SetViewedOwned(authored, owned->baked, 0, "Rotation", {90}).has_value());
    CHECK(Document::KeyAt(authored, "Rotate skew", 0).has_value());
    const Document::PlacementView after = ViewOn(animation, 0, &authored);
    CHECK(Row(after, "Rotation").keying == Document::Keying::KeyedHere);
    CHECK(Row(after, "Scale").keying == Document::Keying::KeyedHere);
}

TEST_CASE("The view shows the anchor of a placement that has one and moves it in place") {
    AfpAnimation::Animation animation = Scene();
    const std::size_t tag = animation.root.frames[0].first_tag;
    auto* create = std::get_if<AfpAnimation::Placement>(&animation.root.tags.at(tag).body);
    REQUIRE(create != nullptr);
    create->origin = std::array<int32_t, 2>{40, 20};

    CheckValues(Row(ViewOn(animation, 1), "Anchor"), {2, 1});
    CHECK(Row(ViewOn(animation, 1), "Anchor").set_on == 0);
    REQUIRE(Document::SetViewedBaked(animation, {}, kDepth, 0, "Anchor", {4, 1}).has_value());
    CheckValues(Row(ViewOn(animation, 0), "Anchor"), {4, 1});
    CHECK(Live(animation, tag).origin == std::array<int32_t, 2>{80, 20});
}

TEST_CASE("A colour the byte range cannot hold is shown as plain channels") {
    AfpAnimation::Animation animation = Scene();
    const std::size_t tag = animation.root.frames[0].first_tag;
    auto* create = std::get_if<AfpAnimation::Placement>(&animation.root.tags.at(tag).body);
    REQUIRE(create != nullptr);
    create->add_colour = std::array<int16_t, 4>{-255, 0, 0, 0};

    const Document::PlacementView view = ViewOn(animation, 0);
    CHECK(Row(view, "Multiply").unit == Document::ViewUnit::Colour);
    CHECK(Row(view, "Add").unit == Document::ViewUnit::Channels);
    CheckValues(Row(view, "Add"), {-255, 0, 0, 0});
}
