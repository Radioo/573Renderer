#include <catch2/catch_test_macros.hpp>

#include "document/clip.h"
#include "document/stage_bounds.h"
#include "document/tags.h"
#include "formats/afp_animation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kThreeD = 0x04000000;
constexpr uint16_t kShape = 1;
constexpr uint16_t kSprite = 2;

using Document::Point;

const std::map<uint16_t, Document::Box> kShapes{
    {kShape, Document::Box{.left = 0, .right = 10, .top = 0, .bottom = 4}}};

AfpAnimation::Container Frames(std::size_t count) {
    AfpAnimation::Container clip;
    for (std::size_t i = 0; i < count; i++)
        clip.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    return clip;
}

AfpAnimation::Placement Create(uint16_t depth, uint16_t character) {
    AfpAnimation::Placement placement;
    placement.flags = kUseMatrix;
    placement.depth = depth;
    placement.character = character;
    return placement;
}

AfpAnimation::Animation WithRoot(AfpAnimation::Container root) {
    AfpAnimation::Animation animation;
    animation.root = std::move(root);
    return animation;
}

std::array<Point, 4> Rect(double left, double top, double right, double bottom) {
    return {Point{left, top}, Point{right, top}, Point{right, bottom}, Point{left, bottom}};
}

}

TEST_CASE("A placed shape is outlined where its matrix puts it, in pixels") {
    AfpAnimation::Container root = Frames(1);
    AfpAnimation::Placement placed = Create(3, kShape);
    placed.scale = std::array<int32_t, 2>{2048, 1024};
    placed.translation = std::array<int32_t, 2>{200, 400};
    Document::InsertTag(root, 0, AfpAnimation::Tag{placed});

    const auto outlines = Document::StageOutlines(WithRoot(root), {}, 0, kShapes);
    REQUIRE(outlines.size() == 1);
    CHECK(outlines[0].depth == 3);
    CHECK(outlines[0].corners == Rect(10, 20, 30, 24));
}

TEST_CASE("The rotation origin is taken off before the matrix and held by updates") {
    AfpAnimation::Container root = Frames(2);
    AfpAnimation::Placement placed = Create(3, kShape);
    placed.flags |= 0x1000000U;
    placed.origin = std::array<int32_t, 2>{100, 40};
    Document::InsertTag(root, 0, AfpAnimation::Tag{placed});
    AfpAnimation::Placement moved;
    moved.flags = kUpdateExisting | kUseMatrix;
    moved.depth = 3;
    moved.translation = std::array<int32_t, 2>{20, 0};
    Document::InsertTag(root, 1, AfpAnimation::Tag{moved});

    const auto outlines = Document::StageOutlines(WithRoot(root), {}, 1, kShapes);
    REQUIRE(outlines.size() == 1);
    CHECK(outlines[0].corners == Rect(-4, -2, 6, 2));
}

TEST_CASE("A sprite is outlined around everything it shows on any of its frames") {
    AfpAnimation::Container sprite = Frames(2);
    Document::InsertTag(sprite, 0, AfpAnimation::Tag{Create(1, kShape)});
    AfpAnimation::Placement later = Create(2, kShape);
    later.translation = std::array<int32_t, 2>{400, 200};
    Document::InsertTag(sprite, 1, AfpAnimation::Tag{later});

    AfpAnimation::Container root = Frames(1);
    root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Sprite{.id = kSprite, .container = sprite}});
    root.frames[0].tag_count = 1;
    AfpAnimation::Placement placed = Create(5, kSprite);
    placed.translation = std::array<int32_t, 2>{20, 20};
    Document::InsertTag(root, 0, AfpAnimation::Tag{placed});

    const auto outlines = Document::StageOutlines(WithRoot(root), {}, 0, kShapes);
    REQUIRE(outlines.size() == 1);
    CHECK(outlines[0].corners == Rect(1, 1, 31, 15));
}

TEST_CASE("A sprite shown on its own is outlined in its own space") {
    AfpAnimation::Container sprite = Frames(1);
    Document::InsertTag(sprite, 0, AfpAnimation::Tag{Create(4, kShape)});
    AfpAnimation::Container root = Frames(1);
    root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Sprite{.id = kSprite, .container = sprite}});
    root.frames[0].tag_count = 1;

    const auto outlines =
        Document::StageOutlines(WithRoot(root), Document::ClipId{.sprite = kSprite}, 0, kShapes);
    REQUIRE(outlines.size() == 1);
    CHECK(outlines[0].depth == 4);
    CHECK(outlines[0].corners == Rect(0, 0, 10, 4));
}

TEST_CASE("Removed, 3D and unknown objects have no outline") {
    AfpAnimation::Container root = Frames(2);
    Document::InsertTag(root, 0, AfpAnimation::Tag{Create(1, kShape)});
    Document::InsertTag(root, 1,
                        AfpAnimation::Tag{AfpAnimation::Remove{.unread_word = 0, .depth = 1}});
    AfpAnimation::Placement deep = Create(2, kShape);
    deep.flags |= kThreeD;
    Document::InsertTag(root, 0, AfpAnimation::Tag{deep});
    Document::InsertTag(root, 0, AfpAnimation::Tag{Create(3, 99)});

    CHECK(Document::StageOutlines(WithRoot(root), {}, 1, kShapes).empty());
    CHECK(Document::StageOutlines(WithRoot(root), {}, 5, kShapes).empty());
}

TEST_CASE("A sprite that places itself does not recurse forever") {
    AfpAnimation::Container sprite = Frames(1);
    Document::InsertTag(sprite, 0, AfpAnimation::Tag{Create(1, kSprite)});
    Document::InsertTag(sprite, 0, AfpAnimation::Tag{Create(2, kShape)});
    AfpAnimation::Container root = Frames(1);
    root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Sprite{.id = kSprite, .container = sprite}});
    root.frames[0].tag_count = 1;
    Document::InsertTag(root, 0, AfpAnimation::Tag{Create(1, kSprite)});

    const auto outlines = Document::StageOutlines(WithRoot(root), {}, 0, kShapes);
    REQUIRE(outlines.size() == 1);
    CHECK(outlines[0].corners == Rect(0, 0, 10, 4));
}

TEST_CASE("A point picks the highest depth whose outline holds it") {
    const std::vector<Document::StageOutline> outlines{
        Document::StageOutline{.depth = 1, .corners = Rect(0, 0, 10, 10)},
        Document::StageOutline{.depth = 4, .corners = Rect(5, 5, 20, 20)},
        Document::StageOutline{
            .depth = 6, .corners = {Point{30, 0}, Point{40, 10}, Point{30, 20}, Point{20, 10}}}};
    CHECK(Document::DepthAt(outlines, {2, 2}) == uint16_t{1});
    CHECK(Document::DepthAt(outlines, {7, 7}) == uint16_t{4});
    CHECK(Document::DepthAt(outlines, {30, 10}) == uint16_t{6});
    CHECK_FALSE(Document::DepthAt(outlines, {21, 1}).has_value());
    CHECK_FALSE(Document::DepthAt(outlines, {50, 50}).has_value());
}
