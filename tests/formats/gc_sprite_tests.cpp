#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "formats/gcanim.h"
#include "formats/sysidx.h"
#include "gc2d/gc_sprite.h"

#include <array>
#include <vector>

namespace {

void FillOneCellPackage(SysIdx::Package& pkg) {
    pkg.cells.push_back(SysIdx::Cell{.x = 0, .y = 0, .w = 32, .h = 16});
    pkg.cell_names["DOT"] = 0;
    SysIdx::Record rec;
    rec.type = SysIdx::kRecDrawCell;
    rec.id = 0;
    rec.t_start = 0;
    rec.t_end = 10;
    pkg.records.push_back(rec);
    SysIdx::Record end;
    end.type = SysIdx::kRecEndAnimation;
    pkg.records.push_back(end);
    pkg.animation_names["SPIN"] = 0;
}

}

TEST_CASE("the 2D canvas decides the device scale and the sprite scale pivot") {
    const Gc2d::Canvas wide{.width = 1280, .height = 720};
    const std::array<float, 2> factors = Gc2d::ScaleFactors(wide, 1280, 720);
    CHECK(factors[0] == Catch::Approx(1.0F));
    CHECK(factors[1] == Catch::Approx(1.0F));

    const std::array<float, 2> pivot = Gc2d::PivotFor(wide, 0.0F, 0.0F);
    CHECK(pivot[0] == Catch::Approx(640.0F));
    CHECK(pivot[1] == Catch::Approx(360.0F));

    const Gc2d::Canvas legacy{};
    CHECK(legacy.width == 640);
    CHECK(legacy.height == 480);
    CHECK(Gc2d::PivotFor(legacy, 0.0F, 0.0F)[0] == Catch::Approx(320.0F));
    CHECK(Gc2d::ScaleFactors(legacy, 1280, 720)[0] == Catch::Approx(2.0F));
}

TEST_CASE("a scroll offset places the scroll where the clock 0 frame starts") {
    SysIdx::Package pkg;
    FillOneCellPackage(pkg);
    const Gc2d::Canvas canvas{};
    Gc2d::SpriteDraw sprite;
    sprite.name = "DOT";
    sprite.x = 200.0F;
    sprite.scroll_x = 1.0F;
    sprite.scroll_wrap = 640.0F;
    sprite.scroll_offset = 100.0F;

    CHECK(Gc2d::ScrollOffset(sprite) == Catch::Approx(100.0F));
    std::vector<GcAnim::DrawNode> nodes;
    Gc2d::AppendNodes(pkg, sprite, canvas, nodes);
    REQUIRE(nodes.size() == 1);
    CHECK(nodes[0].x == Catch::Approx(100.0F));

    sprite.time = 10.0F;
    CHECK(Gc2d::ScrollOffset(sprite) == Catch::Approx(110.0F));

    sprite.time = 600.0F;
    CHECK(Gc2d::ScrollOffset(sprite) == Catch::Approx(60.0F));

    sprite.scroll_wrap = 0.0F;
    CHECK(Gc2d::ScrollOffset(sprite) == Catch::Approx(0.0F));
}

TEST_CASE("a sprite at x 0 on a 1280x720 canvas lands at device pixel 0") {
    SysIdx::Package pkg;
    FillOneCellPackage(pkg);
    const Gc2d::Canvas wide{.width = 1280, .height = 720};
    Gc2d::SpriteDraw sprite;
    sprite.name = "DOT";

    std::vector<GcAnim::DrawNode> nodes;
    Gc2d::AppendNodes(pkg, sprite, wide, nodes);
    REQUIRE(nodes.size() == 1);
    const std::array<float, 2> factors = Gc2d::ScaleFactors(wide, 1280, 720);
    CHECK(nodes[0].x * factors[0] == Catch::Approx(0.0F));
    CHECK(nodes[0].y * factors[1] == Catch::Approx(0.0F));
}

TEST_CASE("a scaled sprite pivots about its own position plus half the canvas") {
    SysIdx::Package pkg;
    FillOneCellPackage(pkg);
    const Gc2d::Canvas wide{.width = 1280, .height = 720};
    Gc2d::SpriteDraw sprite;
    sprite.name = "DOT";
    sprite.scale = 2.0F;

    std::vector<GcAnim::DrawNode> nodes;
    Gc2d::AppendNodes(pkg, sprite, wide, nodes);
    REQUIRE(nodes.size() == 1);
    CHECK(nodes[0].x == Catch::Approx(-640.0F));
    CHECK(nodes[0].y == Catch::Approx(-360.0F));
    CHECK(nodes[0].w == Catch::Approx(64.0F));
}

TEST_CASE("an animated sprite fades its nodes by the layer alpha") {
    SysIdx::Package pkg;
    FillOneCellPackage(pkg);
    const Gc2d::Canvas legacy{};
    Gc2d::SpriteDraw animated;
    animated.name = "SPIN";
    animated.animated = true;
    animated.alpha = 0.5F;

    std::vector<GcAnim::DrawNode> nodes;
    Gc2d::AppendNodes(pkg, animated, legacy, nodes);
    REQUIRE(nodes.size() == 1);
    CHECK(nodes[0].alpha == Catch::Approx(0.5F));

    Gc2d::SpriteDraw cell;
    cell.name = "DOT";
    cell.alpha = 0.25F;
    nodes.clear();
    Gc2d::AppendNodes(pkg, cell, legacy, nodes);
    REQUIRE(nodes.size() == 1);
    CHECK(nodes[0].alpha == Catch::Approx(0.25F));
}
