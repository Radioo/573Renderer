#include "editor/export_range.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("shift dragging the ruler makes an export range in either direction",
          "[editor][export]") {
    const Editor::ExportRange forward = Editor::RangeFromDrag(100, 400, 600);
    CHECK(forward.active);
    CHECK(forward.start == 100);
    CHECK(forward.end == 401);

    const Editor::ExportRange backward = Editor::RangeFromDrag(400, 100, 600);
    CHECK(backward.start == 100);
    CHECK(backward.end == 401);
}

TEST_CASE("an export range is clamped to the frames that exist", "[editor][export]") {
    const Editor::ExportRange clamped =
        Editor::ClampRange(Editor::ExportRange{.start = -30, .end = 900, .active = true}, 600);
    CHECK(clamped.start == 0);
    CHECK(clamped.end == 600);

    const Editor::ExportRange dropped =
        Editor::ClampRange(Editor::ExportRange{.start = 700, .end = 800, .active = true}, 600);
    CHECK_FALSE(dropped.active);
}

TEST_CASE("the exported frame count comes from the range, else the document", "[editor][export]") {
    CHECK(Editor::RangeFrames(Editor::ExportRange{}, 600) == 600);
    CHECK(Editor::RangeFrames(Editor::ExportRange{.start = 100, .end = 401, .active = true}, 600) ==
          301);
}

TEST_CASE("an export fps is honoured only at an integer ratio of the document fps",
          "[editor][export]") {
    CHECK(Editor::FpsRatioAllowed(60, 60));
    CHECK(Editor::FpsRatioAllowed(60, 30));
    CHECK(Editor::FpsRatioAllowed(60, 120));
    CHECK_FALSE(Editor::FpsRatioAllowed(60, 45));
    CHECK_FALSE(Editor::FpsRatioAllowed(60, 25));
    CHECK_FALSE(Editor::FpsRatioAllowed(0, 30));
}
