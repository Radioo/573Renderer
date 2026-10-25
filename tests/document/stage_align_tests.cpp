#include <catch2/catch_test_macros.hpp>

#include "document/stage_align.h"
#include "document/stage_bounds.h"

#include <cstdint>
#include <vector>

namespace {

Document::StageOutline Box(uint16_t depth, double left, double top, double width, double height) {
    return Document::StageOutline{.depth = depth,
                                  .corners = {Document::Point{left, top},
                                              Document::Point{left + width, top},
                                              Document::Point{left + width, top + height},
                                              Document::Point{left, top + height}},
                                  .anchor = {left, top},
                                  .linear = {}};
}

const std::vector<Document::StageOutline> kChosen{Box(1, 100, 50, 40, 20), Box(2, 300, 10, 100, 60),
                                                  Box(3, 20, 200, 10, 10)};

}

TEST_CASE("Aligning lines every chosen depth up with the edge or middle of the whole selection") {
    CHECK(Document::AlignOffsets(kChosen, Document::AlignTo::Left) ==
          std::vector<Document::DepthOffset>{{.depth = 1, .offset = {-80, 0}},
                                             {.depth = 2, .offset = {-280, 0}},
                                             {.depth = 3, .offset = {0, 0}}});
    CHECK(Document::AlignOffsets(kChosen, Document::AlignTo::Right) ==
          std::vector<Document::DepthOffset>{{.depth = 1, .offset = {260, 0}},
                                             {.depth = 2, .offset = {0, 0}},
                                             {.depth = 3, .offset = {370, 0}}});
    CHECK(Document::AlignOffsets(kChosen, Document::AlignTo::HorizontalCentre) ==
          std::vector<Document::DepthOffset>{{.depth = 1, .offset = {90, 0}},
                                             {.depth = 2, .offset = {-140, 0}},
                                             {.depth = 3, .offset = {185, 0}}});
    CHECK(Document::AlignOffsets(kChosen, Document::AlignTo::Top) ==
          std::vector<Document::DepthOffset>{{.depth = 1, .offset = {0, -40}},
                                             {.depth = 2, .offset = {0, 0}},
                                             {.depth = 3, .offset = {0, -190}}});
    CHECK(Document::AlignOffsets(kChosen, Document::AlignTo::Bottom) ==
          std::vector<Document::DepthOffset>{{.depth = 1, .offset = {0, 140}},
                                             {.depth = 2, .offset = {0, 140}},
                                             {.depth = 3, .offset = {0, 0}}});
    CHECK(Document::AlignOffsets(kChosen, Document::AlignTo::VerticalCentre) ==
          std::vector<Document::DepthOffset>{{.depth = 1, .offset = {0, 50}},
                                             {.depth = 2, .offset = {0, 70}},
                                             {.depth = 3, .offset = {0, -95}}});
    CHECK(Document::AlignOffsets({}, Document::AlignTo::Left).empty());
}

TEST_CASE("Spreading keeps the outermost centres and spaces the rest evenly between them") {
    CHECK(Document::SpreadOffsets(kChosen, Document::Spread::Across) ==
          std::vector<Document::DepthOffset>{{.depth = 3, .offset = {0, 0}},
                                             {.depth = 1, .offset = {67.5, 0}},
                                             {.depth = 2, .offset = {0, 0}}});
    CHECK(Document::SpreadOffsets(kChosen, Document::Spread::Down) ==
          std::vector<Document::DepthOffset>{{.depth = 2, .offset = {0, 0}},
                                             {.depth = 1, .offset = {0, 62.5}},
                                             {.depth = 3, .offset = {0, 0}}});
    CHECK(Document::SpreadOffsets({kChosen[0], kChosen[1]}, Document::Spread::Across).empty());
}
