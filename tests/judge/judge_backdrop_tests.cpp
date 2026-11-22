#include <catch2/catch_test_macros.hpp>
#include "judge/judge_backdrop.h"

#include <set>
#include <string>

TEST_CASE("A solid backdrop is one opaque colour with no squares") {
    for (const Judge::Backdrop backdrop : Judge::kBackdrops) {
        if (backdrop == Judge::Backdrop::Checkerboard) continue;
        const Judge::BackdropFill fill = Judge::FillFor(backdrop, 720, 1280);
        CHECK((fill.base >> 24) == 0xFFU);
        CHECK(fill.rects.empty());
    }
    CHECK(Judge::FillFor(Judge::Backdrop::Black, 720, 1280).base == 0xFF000000U);
    CHECK(Judge::FillFor(Judge::Backdrop::White, 720, 1280).base == 0xFFFFFFFFU);
}

TEST_CASE("The checkerboard alternates opaque squares and stays inside the stage") {
    const Judge::BackdropFill fill = Judge::FillFor(Judge::Backdrop::Checkerboard, 40, 20);
    CHECK((fill.base >> 24) == 0xFFU);
    CHECK((fill.squares >> 24) == 0xFFU);
    CHECK(fill.base != fill.squares);
    REQUIRE(fill.rects.size() == 3);
    CHECK(fill.rects[0].x0 == 16);
    CHECK(fill.rects[0].y0 == 0);
    CHECK(fill.rects[0].x1 == 32);
    CHECK(fill.rects[0].y1 == 16);
    CHECK(fill.rects[1].x0 == 0);
    CHECK(fill.rects[1].y0 == 16);
    CHECK(fill.rects[1].y1 == 20);
    CHECK(fill.rects[2].x0 == 32);
    CHECK(fill.rects[2].x1 == 40);
    CHECK(fill.rects[2].y1 == 20);
}

TEST_CASE("A backdrop is found by its name in any case") {
    CHECK(Judge::BackdropByName("checker") == Judge::Backdrop::Checkerboard);
    CHECK(Judge::BackdropByName("Grey") == Judge::Backdrop::Grey);
    CHECK(Judge::BackdropByName("BLUE") == Judge::Backdrop::Blue);
    CHECK_FALSE(Judge::BackdropByName("purple").has_value());
    CHECK_FALSE(Judge::BackdropByName("").has_value());
}

TEST_CASE("Every backdrop has its own name") {
    std::set<std::string> names;
    for (const Judge::Backdrop backdrop : Judge::kBackdrops)
        names.insert(Judge::BackdropName(backdrop));
    CHECK(names.size() == Judge::kBackdrops.size());
}
