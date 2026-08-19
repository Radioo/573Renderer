#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_json.h"
#include "preset/doc/preset_validate.h"
#include "preset/eval/eval_poly.h"
#include "preset/eval/frame_state.h"
#include "preset/eval/preset_evaluator.h"
#include "preset/preset_asset_lengths.h"
#include "preset/preset_rng.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace PD = Preset::Doc;
namespace PE = Preset::Eval;

constexpr float kFrameSeconds = 1.0F / 60.0F;

PD::Document RollDocument(const std::string& tracks, int length) {
    const std::string text = std::string(R"({
  "schema": "573renderer/scene-preset", "version": 1, "id": "roll-doc", "name": "Roll",
  "build": "iidx12", "fps": 60, "length": )") +
                             std::to_string(length) + R"(,
  "render": {"width": 640, "height": 480, "opaque": true, "shading": "lit_material",
             "sprite_split_priority": 30},
  "camera": {"eye": [0, 0, 0], "at": [0, 0, 1], "up": [1, 0, 0], "fov_y": 1.0471976,
             "near_z": 0.0, "far_z": 1000.0, "aspect": "auto"},
  "lights": [],
  "assets": {"sky": {"kind": "scene3d", "dir": "data/graph/model/sky"}},
  "options": [], "rng_seed": 1, "markers": [],
  "tracks": )" + tracks + "}";
    const PD::Loaded loaded = PD::Load(text);
    if (!loaded.has_value()) FAIL(loaded.error().message);
    REQUIRE(loaded.has_value());
    REQUIRE(PD::Validate(*loaded).empty());
    return *loaded;
}

Preset::AssetLengths Lengths() {
    Preset::AssetLengths lengths;
    lengths.scene_ticks["data/graph/model/sky"] = 300.0F;
    return lengths;
}

PE::CameraState CameraAt(const PD::Document& doc, int frame) {
    PE::Evaluator evaluator;
    evaluator.Load(std::make_shared<PD::Document>(doc), Lengths());
    evaluator.Seek(frame);
    return evaluator.Current().camera;
}

PD::PolyTileGrid EndingGrid() {
    return PD::PolyTileGrid{.lattice_seed = 1,
                            .burst_from = 4833,
                            .texture = PD::MovieTexture{.path = "data/movie/08ra.4"}};
}

void CheckCorner(const PE::PolyVec3f& got, const PE::PolyVec3f& want, float margin) {
    CHECK(got[0] == Catch::Approx(want[0]).margin(margin));
    CHECK(got[1] == Catch::Approx(want[1]).margin(margin));
    CHECK(got[2] == Catch::Approx(want[2]).margin(margin));
}
}

TEST_CASE("the CRT generator reproduces the stream the lattice jitter draws from") {
    Preset::CrtRand rng;
    rng.Seed(1);
    const std::vector<int> expected = {41,    18467, 6334, 26500, 19169, 15724, 11478, 29358,
                                       26962, 24464, 5705, 28145, 23281, 16827, 9961,  491};
    for (const int want : expected)
        CHECK(rng.Next() == want);

    rng.Seed(1);
    CHECK(rng.Next() == expected.front());
    rng.Seed(573);
    CHECK(rng.Next() != expected.front());
}

TEST_CASE("camera.motion turns the up vector from +X toward -Y and integrates every frame") {
    const std::string tracks = R"([{"id": "camera", "kind": "camera", "clips": [
        {"id": "hold", "type": "camera.set", "start": 0, "end": 6000,
         "params": {"eye": [0, 0, 0], "at": [0, 0, 1], "up": [1, 0, 0]}},
        {"id": "roll", "type": "camera.motion", "start": 0, "end": 6000,
         "params": {"up_roll_deg_per_frame": 0.2}}]}])";
    const PD::Document doc = RollDocument(tracks, 6000);

    const PE::CameraState start = CameraAt(doc, 0);
    INFO("the clip arms on its first frame and has not turned yet");
    CHECK(start.up[0] == Catch::Approx(1.0F).margin(1e-6));
    CHECK(start.up[1] == Catch::Approx(0.0F).margin(1e-6));

    const PE::CameraState one = CameraAt(doc, 1);
    CHECK(one.up[0] == Catch::Approx(0.9999939F).margin(1e-6));
    CHECK(one.up[1] == Catch::Approx(-0.0034907F).margin(1e-6));
    CHECK(one.up[2] == 0.0F);

    const PE::CameraState turned = CameraAt(doc, 150);
    INFO("150 frames at 0.2 deg is 30 degrees from +X toward -Y");
    CHECK(turned.up[0] == Catch::Approx(0.8660254F).margin(1e-5));
    CHECK(turned.up[1] == Catch::Approx(-0.5F).margin(1e-5));

    const PE::CameraState full = CameraAt(doc, 1800);
    INFO("1800 frames is one whole turn, so the up vector comes back to +X");
    CHECK(full.up[0] == Catch::Approx(1.0F).margin(1e-4));
    CHECK(full.up[1] == Catch::Approx(0.0F).margin(1e-4));

    INFO("the eye and the at point never move");
    CHECK(turned.eye == PE::Vec3f{0.0F, 0.0F, 0.0F});
    CHECK(turned.at == PE::Vec3f{0.0F, 0.0F, 1.0F});
}

TEST_CASE("seeking a rolling camera reproduces the state advancing to it leaves behind") {
    const std::string tracks = R"([{"id": "camera", "kind": "camera", "clips": [
        {"id": "hold", "type": "camera.set", "start": 0, "end": 6000,
         "params": {"eye": [0, 0, 0], "at": [0, 0, 1], "up": [1, 0, 0]}},
        {"id": "roll", "type": "camera.motion", "start": 0, "end": 6000,
         "params": {"up_roll_deg_per_frame": 0.2}}]}])";
    const PD::Document doc = RollDocument(tracks, 6000);

    PE::Evaluator walked;
    walked.Load(std::make_shared<PD::Document>(doc), Lengths());
    for (int frame = 0; frame < 700; frame++)
        walked.RenderFrame(kFrameSeconds);

    PE::Evaluator sought;
    sought.Load(std::make_shared<PD::Document>(doc), Lengths());
    sought.Seek(700);
    CHECK(sought.Current().camera.up == walked.Current().camera.up);

    sought.Seek(120);
    CHECK(sought.Current().camera.up == CameraAt(doc, 120).up);
}

TEST_CASE("the tile lattice jitters twelve of its sixteen points from sixteen CRT draws") {
    const std::vector<PE::PolyVec2f> lattice = PE::LatticePoints(EndingGrid());
    REQUIRE(lattice.size() == 16);

    int used = 0;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            const PE::PolyVec2f point = lattice[((std::size_t)row * 4U) + (std::size_t)col];
            const auto plain_x = (float)col / 3.0F;
            const auto plain_y = (float)row / 3.0F;
            const bool jitter_y = row != 0 && row != 3;
            const bool jitter_x = col != 0 && col != 3;
            if (jitter_y) used++;
            if (jitter_x) used++;
            INFO("lattice point " << row << "," << col);
            CHECK(std::abs(point[1] - plain_y) ==
                  Catch::Approx(jitter_y ? 0.05F : 0.0F).margin(1e-6));
            CHECK(std::abs(point[0] - plain_x) ==
                  Catch::Approx(jitter_x ? 0.05F : 0.0F).margin(1e-6));
        }
    }
    INFO("rows 1 and 2 draw in y, columns 1 and 2 draw in x, the four corners never draw");
    CHECK(used == 16);

    INFO("seed 1 places row 1 low and row 2 col 2 high");
    CHECK(lattice[4][1] == Catch::Approx(0.2833333F).margin(1e-6));
    CHECK(lattice[10][1] == Catch::Approx(0.7166667F).margin(1e-6));
    CHECK(lattice[10][0] == Catch::Approx(0.7166667F).margin(1e-6));

    const PD::PolyTileGrid other = PD::PolyTileGrid{.lattice_seed = 573};
    CHECK(PE::LatticePoints(other) != lattice);
}

TEST_CASE("the tile grid builds nine unrotated quads at frame zero") {
    const PD::PolyTileGrid grid = EndingGrid();
    const std::vector<PE::PolyQuad> quads = PE::PolyQuadsAt(grid, PE::LatticePoints(grid), 0);
    REQUIRE(quads.size() == 9);

    INFO("quad 0 is centred on (-3, -2.25, 5) and is 1.2 by 0.9 before the jitter");
    CheckCorner(quads[0].corners[0], {-3.6F, -1.935F, 5.0F}, 1e-4F);
    CheckCorner(quads[0].corners[1], {-2.22F, -1.935F, 5.0F}, 1e-4F);
    CheckCorner(quads[0].corners[2], {-3.6F, -2.7F, 5.0F}, 1e-4F);
    CheckCorner(quads[0].corners[3], {-2.22F, -2.7F, 5.0F}, 1e-4F);
    CheckCorner(quads[8].corners[0], {2.58F, 2.7F, 5.0F}, 1e-4F);

    INFO("the uv covers the movie's 304 by 416 inside a 512 square, one cell per quad");
    CHECK(quads[0].uv[0][0] == Catch::Approx(0.0F).margin(1e-6));
    CHECK(quads[0].uv[0][1] == Catch::Approx(0.2302083F).margin(1e-6));
    CHECK(quads[0].uv[1][0] == Catch::Approx(0.2276042F).margin(1e-6));
    CHECK(quads[0].uv[3][1] == Catch::Approx(0.0F).margin(1e-6));
}

TEST_CASE("the tile grid spins and orbits every quad by the frame counter") {
    const PD::PolyTileGrid grid = EndingGrid();
    const std::vector<PE::PolyVec2f> lattice = PE::LatticePoints(grid);

    const std::vector<PE::PolyQuad> at_200 = PE::PolyQuadsAt(grid, lattice, 200);
    REQUIRE(at_200.size() == 9);
    INFO("hand transcribed from the game's stage 1 to 3 arithmetic at frame 200");
    CheckCorner(at_200[0].corners[0], {-2.33472F, 0.52712F, 2.40187F}, 2e-3F);
    CheckCorner(at_200[0].corners[3], {-3.51052F, 1.32149F, 3.09187F}, 2e-3F);
    CheckCorner(at_200[8].corners[0], {2.29209F, 2.7F, 7.08813F}, 2e-3F);

    const std::vector<PE::PolyQuad> at_201 = PE::PolyQuadsAt(grid, lattice, 201);
    CheckCorner(at_201[0].corners[0], {-2.32618F, 0.52787F, 2.4003F}, 2e-3F);

    const std::vector<PE::PolyQuad> at_4833 = PE::PolyQuadsAt(grid, lattice, 4833);
    CheckCorner(at_4833[0].corners[0], {-1.59894F, 1.70587F, 4.39285F}, 5e-3F);

    INFO("quad 8 only ever turns about Y, so its corner heights never change");
    CHECK(at_200[8].corners[0][1] == Catch::Approx(2.7F).margin(1e-4));
    CHECK(at_4833[8].corners[0][1] == Catch::Approx(2.7F).margin(1e-4));
}

TEST_CASE("an odd parity quad takes the X and Z spins the corner quads never take") {
    const PD::PolyTileGrid grid = EndingGrid();
    const std::vector<PE::PolyVec2f> lattice = PE::LatticePoints(grid);
    const std::vector<PE::PolyQuad> at_200 = PE::PolyQuadsAt(grid, lattice, 200);
    REQUIRE(at_200.size() == 9);

    INFO("quad 1 is row 0 column 1, the only quad pinned here whose col % 2 and (row + col) % 2 "
         "are both 1: at frame 200 it spins 200 deg about X, 100 about Y and -400 about Z");
    CheckCorner(at_200[1].corners[0], {0.43715F, -2.49807F, 4.84840F}, 2e-3F);
    CheckCorner(at_200[1].corners[1], {-0.26837F, -2.59183F, 5.29454F}, 2e-3F);
    CheckCorner(at_200[1].corners[2], {0.23221F, -1.78176F, 4.67484F}, 2e-3F);
    CheckCorner(at_200[1].corners[3], {-0.77569F, -1.91570F, 5.31218F}, 2e-3F);

    PD::PolyTileGrid retimed = grid;
    retimed.spin_rates = PD::Vec3{2.0, 1.0, -4.0};
    const std::vector<PE::PolyQuad> altered = PE::PolyQuadsAt(retimed, lattice, 200);
    REQUIRE(altered.size() == 9);
    INFO("quads 0 and 8 have even parity in both terms, so the X and Z rates cannot reach them: "
         "only an odd parity quad can hold those two rates to their values");
    CHECK(altered[0].corners == at_200[0].corners);
    CHECK(altered[8].corners == at_200[8].corners);
    CHECK(altered[1].corners != at_200[1].corners);
}

TEST_CASE("the tile burst pushes each quad in +z on its own staggered schedule") {
    PD::PolyTileGrid grid = EndingGrid();
    const std::vector<PE::PolyVec2f> lattice = PE::LatticePoints(grid);

    PD::PolyTileGrid quiet = grid;
    quiet.burst_from = 1000000;

    INFO("the counter is zero right up to the frame it starts on");
    CHECK(PE::PolyQuadsAt(grid, lattice, 4832)[0].corners[0][2] ==
          Catch::Approx(PE::PolyQuadsAt(quiet, lattice, 4832)[0].corners[0][2]).margin(1e-4));

    INFO("the first ramped frame already carries one 2.0 step");
    CHECK(PE::PolyQuadsAt(grid, lattice, 4833)[0].corners[0][2] -
              PE::PolyQuadsAt(quiet, lattice, 4833)[0].corners[0][2] ==
          Catch::Approx(2.0F).margin(1e-3));

    const std::vector<PE::PolyQuad> late = PE::PolyQuadsAt(grid, lattice, 4900);
    const std::vector<PE::PolyQuad> still = PE::PolyQuadsAt(quiet, lattice, 4900);
    INFO("at frame 4900 the counter is 136 and quad 8 waits out 80 of it");
    CHECK(late[0].corners[0][2] - still[0].corners[0][2] == Catch::Approx(136.0F).margin(1e-3));
    CHECK(late[8].corners[0][2] - still[8].corners[0][2] == Catch::Approx(56.0F).margin(1e-3));
    CheckCorner(late[0].corners[0], {-1.79387F, 2.41376F, 138.69972F}, 1e-2F);

    grid.burst_delay_per_tile = 0.0;
    const std::vector<PE::PolyQuad> together = PE::PolyQuadsAt(grid, lattice, 4900);
    CHECK(together[8].corners[0][2] - still[8].corners[0][2] == Catch::Approx(136.0F).margin(1e-3));
}
