#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "document/stage_bounds.h"
#include "document/transform_parts.h"

#include <numbers>

using Catch::Matchers::WithinAbs;

namespace {

constexpr double kClose = 1e-9;

void CheckSame(const Document::Linear& got, const Document::Linear& wanted) {
    CHECK_THAT(got.a, WithinAbs(wanted.a, kClose));
    CHECK_THAT(got.b, WithinAbs(wanted.b, kClose));
    CHECK_THAT(got.c, WithinAbs(wanted.c, kClose));
    CHECK_THAT(got.d, WithinAbs(wanted.d, kClose));
}

}

TEST_CASE("An unchanged object has full scale, no rotation and no skew") {
    const Document::TransformParts parts = Document::PartsOf(Document::Linear{});
    CHECK_THAT(parts.scale_x, WithinAbs(1, kClose));
    CHECK_THAT(parts.scale_y, WithinAbs(1, kClose));
    CHECK_THAT(parts.rotation, WithinAbs(0, kClose));
    CHECK_THAT(parts.skew, WithinAbs(0, kClose));
}

TEST_CASE("A turn made on stage reads back as that rotation in degrees") {
    const Document::Linear turned = Document::Reshaped(
        Document::Linear{}, {.scale_x = 2, .scale_y = 0.5, .turn = std::numbers::pi / 6});
    const Document::TransformParts parts = Document::PartsOf(turned);
    CHECK_THAT(parts.scale_x, WithinAbs(2, kClose));
    CHECK_THAT(parts.scale_y, WithinAbs(0.5, kClose));
    CHECK_THAT(parts.rotation, WithinAbs(30, kClose));
    CHECK_THAT(parts.skew, WithinAbs(0, kClose));
    CheckSame(Document::LinearOf(parts), turned);
}

TEST_CASE("A horizontal flip reads as a negative width, not as a turn") {
    const Document::Linear flipped =
        Document::Reshaped(Document::Linear{}, {.scale_x = -1, .scale_y = 1, .turn = 0});
    const Document::TransformParts parts = Document::PartsOf(flipped);
    CHECK_THAT(parts.scale_x, WithinAbs(-1, kClose));
    CHECK_THAT(parts.scale_y, WithinAbs(1, kClose));
    CHECK_THAT(parts.rotation, WithinAbs(0, kClose));
    CHECK_THAT(parts.skew, WithinAbs(0, kClose));
    CheckSame(Document::LinearOf(parts), flipped);
}

TEST_CASE("A skewed and turned matrix comes back exactly from its parts") {
    const Document::Linear skewed{.a = 0.8, .b = 0.3, .c = -0.9, .d = 1.4};
    const Document::TransformParts parts = Document::PartsOf(skewed);
    CheckSame(Document::LinearOf(parts), skewed);
    Document::TransformParts turned = parts;
    turned.rotation += 45;
    const Document::TransformParts again = Document::PartsOf(Document::LinearOf(turned));
    CHECK_THAT(again.rotation, WithinAbs(parts.rotation + 45, kClose));
    CHECK_THAT(again.skew, WithinAbs(parts.skew, kClose));
    CHECK_THAT(again.scale_x, WithinAbs(parts.scale_x, kClose));
    CHECK_THAT(again.scale_y, WithinAbs(parts.scale_y, kClose));
}

TEST_CASE("Rotation and skew are kept between minus and plus half a turn") {
    const Document::Linear upside_down{.a = -1, .b = 0, .c = 0, .d = -1};
    const Document::TransformParts parts = Document::PartsOf(upside_down);
    CHECK_THAT(parts.rotation, WithinAbs(180, kClose));
    CHECK_THAT(parts.skew, WithinAbs(0, kClose));
    CHECK(parts.scale_x > 0);
    CheckSame(Document::LinearOf(parts), upside_down);
}
