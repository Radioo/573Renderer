#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "document/stage_bounds.h"

using Catch::Matchers::WithinAbs;

namespace {

Document::StageOutline Placed(double x, double y, double scale) {
    return Document::StageOutline{.depth = 1,
                                  .corners = {Document::Point{x, y}, Document::Point{x + scale, y},
                                              Document::Point{x + scale, y + scale},
                                              Document::Point{x, y + scale}},
                                  .anchor = {x, y},
                                  .linear = {.a = scale, .b = 0, .c = 0, .d = scale}};
}

}

TEST_CASE("A point in a clip's own space is placed through the clip's outline and back") {
    const Document::StageOutline through = Placed(100, 50, 2);
    const Document::Point placed = Document::ThroughOutline(through, Document::Point{10, 20});
    CHECK_THAT(placed[0], WithinAbs(120, 1e-9));
    CHECK_THAT(placed[1], WithinAbs(90, 1e-9));
    const auto back = Document::UnderOutline(through, placed);
    REQUIRE(back.has_value());
    CHECK_THAT((*back)[0], WithinAbs(10, 1e-9));
    CHECK_THAT((*back)[1], WithinAbs(20, 1e-9));
}

TEST_CASE("A clip flattened to nothing cannot be mapped back into") {
    Document::StageOutline through = Placed(0, 0, 1);
    through.linear = Document::Linear{.a = 0, .b = 0, .c = 0, .d = 0};
    CHECK_FALSE(Document::UnderOutline(through, Document::Point{5, 5}).has_value());
}

TEST_CASE("An outline inside a clip carries the clip's own scale and place") {
    const Document::StageOutline through = Placed(100, 50, 2);
    const Document::StageOutline inside = Placed(5, 5, 3);
    const Document::StageOutline mapped = Document::OutlineThrough(inside, through);
    CHECK(mapped.depth == inside.depth);
    CHECK_THAT(mapped.anchor[0], WithinAbs(110, 1e-9));
    CHECK_THAT(mapped.anchor[1], WithinAbs(60, 1e-9));
    CHECK_THAT(mapped.corners[2][0], WithinAbs(116, 1e-9));
    CHECK_THAT(mapped.corners[2][1], WithinAbs(66, 1e-9));
    CHECK_THAT(mapped.linear.a, WithinAbs(6, 1e-9));
    CHECK_THAT(mapped.linear.d, WithinAbs(6, 1e-9));
    CHECK_THAT(mapped.linear.b, WithinAbs(0, 1e-9));
}

TEST_CASE("A turned clip turns what is inside it") {
    Document::StageOutline through = Placed(0, 0, 1);
    through.linear = Document::Linear{.a = 0, .b = 1, .c = -1, .d = 0};
    const Document::Point placed = Document::ThroughOutline(through, Document::Point{2, 0});
    CHECK_THAT(placed[0], WithinAbs(0, 1e-9));
    CHECK_THAT(placed[1], WithinAbs(2, 1e-9));
    const Document::StageOutline mapped = Document::OutlineThrough(Placed(0, 0, 1), through);
    CHECK_THAT(mapped.linear.b, WithinAbs(1, 1e-9));
    CHECK_THAT(mapped.linear.a, WithinAbs(0, 1e-9));
}
