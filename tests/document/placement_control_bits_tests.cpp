#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "document/clip_edit.h"
#include "document/placement_effect.h"
#include "document/stage_bounds.h"
#include "formats/afp_animation.h"

#include <array>
#include <cstdint>
#include <map>
#include <utility>
#include <variant>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kUseColour = 0x8;
constexpr uint16_t kShape = 1;

const std::map<uint16_t, Document::Box> kShapes{
    {kShape, Document::Box{.left = 0, .right = 10, .top = 0, .bottom = 4}}};

AfpAnimation::Placement Create(uint16_t depth, uint32_t flags) {
    AfpAnimation::Placement placement;
    placement.flags = flags;
    placement.depth = depth;
    placement.character = kShape;
    return placement;
}

AfpAnimation::Placement Update(uint16_t depth, uint32_t flags) {
    AfpAnimation::Placement placement;
    placement.flags = kUpdateExisting | flags;
    placement.depth = depth;
    return placement;
}

AfpAnimation::Animation Of(const std::vector<std::vector<AfpAnimation::Tag>>& per_frame) {
    AfpAnimation::Animation animation;
    for (const std::vector<AfpAnimation::Tag>& tags : per_frame) {
        animation.root.frames.push_back(
            AfpAnimation::Frame{.first_tag = static_cast<uint32_t>(animation.root.tags.size()),
                                .tag_count = static_cast<uint32_t>(tags.size())});
        for (const AfpAnimation::Tag& tag : tags)
            animation.root.tags.push_back(tag);
    }
    return animation;
}

Document::Linear LinearOn(const AfpAnimation::Animation& animation, uint16_t depth,
                          uint32_t frame) {
    const std::vector<Document::StageOutline> outlines =
        Document::StageOutlines(animation, {}, frame, kShapes);
    for (const Document::StageOutline& outline : outlines) {
        if (outline.depth == depth) return outline.linear;
    }
    return Document::Linear{.a = 0, .b = 0, .c = 0, .d = 0};
}

Document::AppliedState AppliedOn(const AfpAnimation::Animation& animation, uint16_t depth,
                                 uint32_t frame) {
    const auto shown = Document::ReplayDepth(animation.root, depth, frame, frame);
    REQUIRE_FALSE(shown.empty());
    return shown.back().second;
}

}

TEST_CASE("A scale written on a placement that carries no matrix is drawn") {
    AfpAnimation::Animation animation = Of({{AfpAnimation::Tag{Create(1, 0)}}});
    REQUIRE(LinearOn(animation, 1, 0).a == 1);

    const auto written = Document::EditPlacementField(animation, {}, 1, 0, "Scale", "2048, 2048");
    REQUIRE(written.has_value());
    const Document::Linear linear = LinearOn(animation, 1, 0);
    CHECK_THAT(linear.a, WithinAbs(2, 1e-9));
    CHECK_THAT(linear.d, WithinAbs(2, 1e-9));
}

TEST_CASE("A colour written on a placement that carries none is applied") {
    AfpAnimation::Animation animation = Of({{AfpAnimation::Tag{Create(1, 0)}}});
    REQUIRE_THAT(AppliedOn(animation, 1, 0).multiply[0], WithinAbs(1, 1e-9));

    const auto written =
        Document::EditPlacementField(animation, {}, 1, 0, "Multiply colour", "128, 255, 255, 255");
    REQUIRE(written.has_value());
    CHECK_THAT(AppliedOn(animation, 1, 0).multiply[0], WithinAbs(128.0 / 255.0, 1e-9));
}

TEST_CASE("Writing a matrix field on an update keeps what the depth was already drawn with") {
    AfpAnimation::Placement create = Create(1, kUseMatrix);
    create.scale = std::array<int32_t, 2>{2048, 2048};
    create.translation = std::array<int32_t, 2>{400, 800};
    AfpAnimation::Animation animation =
        Of({{AfpAnimation::Tag{create}}, {AfpAnimation::Tag{Update(1, 0)}}});

    const auto written =
        Document::EditPlacementField(animation, {}, 1, 1, "Rotate skew", "0, 1024");
    REQUIRE(written.has_value());
    const Document::AppliedState applied = AppliedOn(animation, 1, 1);
    CHECK_THAT(applied.matrix[0], WithinAbs(2, 1e-9));
    CHECK_THAT(applied.matrix[3], WithinAbs(2, 1e-9));
    CHECK_THAT(applied.matrix[2], WithinAbs(1, 1e-9));
    CHECK_THAT(applied.matrix[4], WithinAbs(400, 1e-9));
    CHECK_THAT(applied.matrix[5], WithinAbs(800, 1e-9));
}

TEST_CASE("Writing a colour field on an update keeps the colour the depth was already drawn with") {
    AfpAnimation::Placement create = Create(1, kUseColour);
    create.multiply_colour = std::array<int16_t, 4>{128, 128, 128, 255};
    AfpAnimation::Animation animation =
        Of({{AfpAnimation::Tag{create}}, {AfpAnimation::Tag{Update(1, 0)}}});

    const auto written =
        Document::EditPlacementField(animation, {}, 1, 1, "Add colour", "16, 0, 0, 0");
    REQUIRE(written.has_value());
    const Document::AppliedState applied = AppliedOn(animation, 1, 1);
    CHECK_THAT(applied.multiply[0], WithinAbs(128.0 / 255.0, 1e-9));
    CHECK_THAT(applied.add[0], WithinAbs(16.0 / 255.0, 1e-9));
}

TEST_CASE("A field outside the matrix and the colour leaves the control bits alone") {
    AfpAnimation::Animation animation = Of({{AfpAnimation::Tag{Create(1, 0)}}});
    const auto written = Document::EditPlacementField(animation, {}, 1, 0, "Blend", "4");
    REQUIRE(written.has_value());
    const auto* placement = std::get_if<AfpAnimation::Placement>(&animation.root.tags.front().body);
    REQUIRE(placement != nullptr);
    CHECK((placement->flags & kUseMatrix) == 0);
    CHECK((placement->flags & kUseColour) == 0);
}
