#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "preset/doc/preset_commands.h"
#include "preset/eval/eval_poly.h"
#include "scene3d/poly_grid.h"
#include "scene3d/poly_vertex.h"

#include <array>
#include <cstddef>
#include <vector>

namespace {

namespace PD = Preset::Doc;
namespace PE = Preset::Eval;

PD::PolyTileGrid EndingGrid() {
    return PD::PolyTileGrid{.lattice_seed = 1,
                            .burst_from = 4833,
                            .texture = PD::MovieTexture{.path = "data/movie/08ra.4"}};
}

Scene3d::PolyTile TileAt(std::size_t index) {
    const PD::PolyTileGrid grid = EndingGrid();
    const std::vector<PE::PolyQuad> quads = PE::PolyQuadsAt(grid, PE::LatticePoints(grid), 200);
    REQUIRE(quads.size() > index);
    Scene3d::PolyTile tile;
    tile.corners = quads[index].corners;
    tile.uv = quads[index].uv;
    return tile;
}

}

TEST_CASE("the tile pass turns the grid alpha into a diffuse over white") {
    INFO("the ending's 127 of 255 is the game's own half alpha and must survive the round trip");
    CHECK(Scene3d::DiffuseOf(127.0F / 255.0F) == 0x7FFFFFFFU);
    CHECK(Scene3d::DiffuseOf(0.0F) == 0x00FFFFFFU);
    CHECK(Scene3d::DiffuseOf(1.0F) == 0xFFFFFFFFU);

    INFO("the colour is always white, and the level is clamped rather than wrapped");
    CHECK(Scene3d::DiffuseOf(4.0F) == 0xFFFFFFFFU);
    CHECK(Scene3d::DiffuseOf(-1.0F) == 0x00FFFFFFU);
}

TEST_CASE("a tile strip is four XYZ NORMAL DIFFUSE TEX1 vertices facing +Y") {
    const Scene3d::PolyTile tile = TileAt(4);
    const std::array<Scene3d::PolyVertex, 4> strip = Scene3d::StripOf(tile, 0x7FFFFFFFU);

    INFO("the fixed function layout the FVF promises is 3 + 3 floats, a colour, then 2 floats");
    CHECK(sizeof(Scene3d::PolyVertex) == 36);
    CHECK(Scene3d::kPolyFvf == 0x152U);
    CHECK(offsetof(Scene3d::PolyVertex, nx) == 12);
    CHECK(offsetof(Scene3d::PolyVertex, diffuse) == 24);
    CHECK(offsetof(Scene3d::PolyVertex, u) == 28);

    for (std::size_t i = 0; i < strip.size(); i++) {
        INFO("vertex " << i);
        CHECK(strip[i].nx == 0.0F);
        CHECK(strip[i].ny == 1.0F);
        CHECK(strip[i].nz == 0.0F);
        CHECK(strip[i].diffuse == 0x7FFFFFFFU);
        CHECK(strip[i].x == tile.corners[i][0]);
        CHECK(strip[i].y == tile.corners[i][1]);
        CHECK(strip[i].z == tile.corners[i][2]);
    }
}

TEST_CASE("a tile strip carries the uv its own quad cut out of the movie") {
    const Scene3d::PolyTile tile = TileAt(4);
    const std::array<Scene3d::PolyVertex, 4> strip = Scene3d::StripOf(tile, 0);

    INFO("quad 4 is row 1 column 1, and its four lattice points scale by 304/512 and 416/512");
    CHECK(strip[0].u == Catch::Approx(0.2276042F).margin(1e-6));
    CHECK(strip[0].v == Catch::Approx(0.5010417F).margin(1e-6));
    CHECK(strip[1].u == Catch::Approx(0.4255208F).margin(1e-6));
    CHECK(strip[1].v == Catch::Approx(0.5822917F).margin(1e-6));
    CHECK(strip[2].u == Catch::Approx(0.2276042F).margin(1e-6));
    CHECK(strip[2].v == Catch::Approx(0.2302083F).margin(1e-6));
    CHECK(strip[3].u == Catch::Approx(0.3661458F).margin(1e-6));
    CHECK(strip[3].v == Catch::Approx(0.2302083F).margin(1e-6));

    INFO("no two of the four agree in both axes, so a swapped or transposed uv cannot pass");
    for (std::size_t i = 0; i < strip.size(); i++) {
        for (std::size_t k = i + 1; k < strip.size(); k++) {
            INFO("vertices " << i << " and " << k);
            const bool same = (strip[i].u == strip[k].u) && (strip[i].v == strip[k].v);
            CHECK_FALSE(same);
        }
    }
}
