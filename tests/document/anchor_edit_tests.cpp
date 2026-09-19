#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/anchor_edit.h"
#include "document/stage_bounds.h"
#include "document/tags.h"
#include "formats/afp_animation.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kUseColour = 0x8;
constexpr uint32_t kThreeD = 0x04000000;
constexpr uint16_t kShape = 1;
constexpr uint16_t kDepth = 3;

const std::map<uint16_t, Document::Box> kShapes{
    {kShape, Document::Box{.left = 2, .right = 12, .top = 1, .bottom = 5}}};

AfpAnimation::Animation Frames(std::size_t count) {
    AfpAnimation::Animation animation;
    for (std::size_t i = 0; i < count; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    return animation;
}

AfpAnimation::Placement Create(uint32_t flags) {
    AfpAnimation::Placement placement;
    placement.flags = flags;
    placement.depth = kDepth;
    placement.character = kShape;
    return placement;
}

AfpAnimation::Placement Update(uint32_t flags) {
    AfpAnimation::Placement placement;
    placement.flags = kUpdateExisting | flags;
    placement.depth = kDepth;
    return placement;
}

void Put(AfpAnimation::Animation& animation, uint32_t frame,
         const AfpAnimation::Placement& placement) {
    Document::InsertTag(animation.root, frame, AfpAnimation::Tag{placement});
}

AfpAnimation::Animation Scene() {
    AfpAnimation::Animation animation = Frames(4);
    AfpAnimation::Placement placed = Create(kUseMatrix);
    placed.scale = std::array<int32_t, 2>{2048, 1024};
    placed.translation = std::array<int32_t, 2>{200, 400};
    Put(animation, 0, placed);
    AfpAnimation::Placement turned = Update(kUseMatrix);
    turned.scale = std::array<int32_t, 2>{1024, 2048};
    turned.rotate_skew = std::array<int32_t, 2>{512, -512};
    turned.translation = std::array<int32_t, 2>{300, 400};
    Put(animation, 1, turned);
    AfpAnimation::Placement tinted = Update(kUseColour);
    tinted.multiply_colour = std::array<int16_t, 4>{128, 256, 256, 256};
    Put(animation, 2, tinted);
    AfpAnimation::Placement recentred = Update(kUseMatrix);
    recentred.origin = std::array<int32_t, 2>{40, 20};
    recentred.translation = std::array<int32_t, 2>{500, 100};
    Put(animation, 3, recentred);
    return animation;
}

Document::StageOutline OutlineOn(const AfpAnimation::Animation& animation, uint32_t frame) {
    const auto outlines = Document::StageOutlines(animation, {}, frame, kShapes);
    REQUIRE(outlines.size() == 1);
    return outlines.front();
}

bool Near(const Document::Point& a, const Document::Point& b) {
    return std::abs(a[0] - b[0]) < 0.06 && std::abs(a[1] - b[1]) < 0.06;
}

const AfpAnimation::Placement& PlacementAt(const AfpAnimation::Animation& animation,
                                           std::size_t index) {
    const auto* placement =
        std::get_if<AfpAnimation::Placement>(&animation.root.tags.at(index).body);
    REQUIRE(placement != nullptr);
    return *placement;
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

}

TEST_CASE("Centring the anchor moves it onto the content and keeps every frame where it was") {
    AfpAnimation::Animation animation = Scene();
    std::vector<Document::StageOutline> before;
    before.reserve(4);
    for (uint32_t frame = 0; frame < 4; frame++)
        before.push_back(OutlineOn(animation, frame));
    const auto centred = Document::CentreAnchor(animation, {}, kDepth, 1, kShapes);
    INFO(Error(centred));
    REQUIRE(centred.has_value());
    for (uint32_t frame = 0; frame < 4; frame++) {
        INFO(frame);
        const Document::StageOutline after = OutlineOn(animation, frame);
        for (std::size_t corner = 0; corner < 4; corner++)
            CHECK(Near(after.corners.at(corner), before.at(frame).corners.at(corner)));
    }
    CHECK(Near(OutlineOn(animation, 0).anchor, Document::Point{24, 23}));
    CHECK(PlacementAt(animation, 0).origin == std::array<int32_t, 2>{140, 60});
    CHECK(PlacementAt(animation, 3).origin == std::array<int32_t, 2>{180, 80});
    CHECK(PlacementAt(animation, 2) ==
          std::get<AfpAnimation::Placement>(Scene().root.tags[2].body));
}

TEST_CASE("Centring the anchor of an object placed without a matrix gives it one") {
    AfpAnimation::Animation animation = Frames(2);
    Put(animation, 0, Create(0));
    const auto centred = Document::CentreAnchor(animation, {}, kDepth, 0, kShapes);
    INFO(Error(centred));
    REQUIRE(centred.has_value());
    const AfpAnimation::Placement& placed = PlacementAt(animation, 0);
    CHECK((placed.flags & kUseMatrix) != 0);
    CHECK(placed.origin == std::array<int32_t, 2>{140, 60});
    CHECK(placed.translation == std::array<int32_t, 2>{140, 60});
    const Document::StageOutline outline = OutlineOn(animation, 1);
    CHECK(Near(outline.corners[0], Document::Point{2, 1}));
    CHECK(Near(outline.corners[2], Document::Point{12, 5}));
    CHECK(Near(outline.anchor, Document::Point{7, 3}));
}

TEST_CASE(
    "Centring the anchor is refused, leaving the clip alone, where it cannot keep the frames") {
    AfpAnimation::Animation animation = Scene();
    const auto refusal = [&animation](uint32_t frame,
                                      const std::map<uint16_t, Document::Box>& shapes) {
        return Error(Document::CentreAnchor(animation, {}, kDepth, frame, shapes));
    };
    CHECK(refusal(9, kShapes).find("holds nothing") != std::string::npos);
    CHECK(refusal(0, {}).find("no centre") != std::string::npos);
    Put(animation, 2, Update(kUseMatrix | kThreeD));
    const AfpAnimation::Animation with_3d = animation;
    CHECK(refusal(0, kShapes).find("3D") != std::string::npos);
    CHECK(animation == with_3d);

    animation = Frames(2);
    AfpAnimation::Placement drawn = Create(kUseMatrix);
    drawn.geometry = 1;
    Put(animation, 0, drawn);
    CHECK(refusal(0, kShapes).find("geometry") != std::string::npos);

    animation = Scene();
    REQUIRE(Document::CentreAnchor(animation, {}, kDepth, 0, kShapes).has_value());
    const AfpAnimation::Animation centred = animation;
    CHECK(refusal(0, kShapes).find("already") != std::string::npos);
    CHECK(animation == centred);
}
