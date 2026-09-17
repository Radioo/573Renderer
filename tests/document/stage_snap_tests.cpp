#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "document/stage_bounds.h"
#include "document/stage_snap.h"

#include <cstdint>
#include <vector>

namespace {

using Catch::Matchers::WithinAbs;

Document::StageOutline Square(uint16_t depth, double left, double top, double size) {
    return Document::StageOutline{
        .depth = depth,
        .corners = {Document::Point{left, top}, Document::Point{left + size, top},
                    Document::Point{left, top + size}, Document::Point{left + size, top + size}},
        .anchor = {left, top},
        .linear = {}};
}

constexpr Document::Point kStage{1920, 1080};

}

TEST_CASE("A moved object snaps its edges and centre to the stage's") {
    const Document::StageOutline moving = Square(1, 100, 100, 200);
    const Document::Snapped left =
        Document::SnapMove(moving, {}, kStage, Document::Point{-96, 0}, 8);
    CHECK_THAT(left.offset[0], WithinAbs(-100, 1e-9));
    CHECK_THAT(left.offset[1], WithinAbs(0, 1e-9));
    REQUIRE(left.guides.size() == 1);
    CHECK(left.guides[0] == Document::SnapGuide{.vertical = true, .at = 0});

    const Document::Snapped centred =
        Document::SnapMove(moving, {}, kStage, Document::Point{757, 337}, 8);
    CHECK_THAT(centred.offset[0], WithinAbs(760, 1e-9));
    CHECK_THAT(centred.offset[1], WithinAbs(340, 1e-9));
    CHECK(centred.guides == std::vector<Document::SnapGuide>{{.vertical = true, .at = 960},
                                                             {.vertical = false, .at = 540}});
}

TEST_CASE("A moved object snaps to other objects but not to itself or from far away") {
    const Document::StageOutline moving = Square(1, 100, 100, 200);
    const std::vector<Document::StageOutline> others{moving, Square(2, 500, 700, 100)};
    const Document::Snapped beside =
        Document::SnapMove(moving, others, kStage, Document::Point{203, 13}, 8);
    CHECK_THAT(beside.offset[0], WithinAbs(200, 1e-9));
    CHECK_THAT(beside.offset[1], WithinAbs(13, 1e-9));
    CHECK(beside.guides == std::vector<Document::SnapGuide>{{.vertical = true, .at = 500}});

    const Document::Snapped free =
        Document::SnapMove(moving, others, kStage, Document::Point{50, 50}, 8);
    CHECK_THAT(free.offset[0], WithinAbs(50, 1e-9));
    CHECK_THAT(free.offset[1], WithinAbs(50, 1e-9));
    CHECK(free.guides.empty());
}

TEST_CASE("The nearest line wins when several are within reach") {
    const Document::StageOutline moving = Square(1, 0, 0, 100);
    const std::vector<Document::StageOutline> others{Square(2, 300, 900, 10),
                                                     Square(3, 305, 900, 10)};
    const Document::Snapped snapped =
        Document::SnapMove(moving, others, kStage, Document::Point{204, 500}, 8);
    CHECK_THAT(snapped.offset[0], WithinAbs(205, 1e-9));
    CHECK(snapped.guides == std::vector<Document::SnapGuide>{{.vertical = true, .at = 305}});
}
