#include "formats/gcz.h"
#include "formats/inz.h"
#include "scene3d/atlas.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr int kSwatchRows = 8;
constexpr int kSliceWidth = 8;
constexpr int kSliceHeight = 16;
constexpr uint16_t kOpaqueRed = 0xE863U;
constexpr uint16_t kPackerFill = 0x03E0U;

void PushBe16(std::vector<uint8_t>& v, uint16_t value) {
    v.push_back((uint8_t)(value >> 8));
    v.push_back((uint8_t)(value & 0xFFU));
}

void PushBe32(std::vector<uint8_t>& v, uint32_t value) {
    v.push_back((uint8_t)(value >> 24));
    v.push_back((uint8_t)((value >> 16) & 0xFFU));
    v.push_back((uint8_t)((value >> 8) & 0xFFU));
    v.push_back((uint8_t)(value & 0xFFU));
}

std::vector<uint8_t> PackedSliceWithFiller() {
    std::vector<uint8_t> v = {'G', 'C', ' ', 0};
    PushBe32(v, 0);
    PushBe16(v, 0);
    PushBe16(v, 0);
    PushBe16(v, kSliceWidth);
    PushBe16(v, kSliceHeight);
    PushBe32(v, 0);
    PushBe32(v, (uint32_t)(kSliceWidth * kSliceHeight * 2));
    for (int y = 0; y < kSliceHeight; y++) {
        for (int x = 0; x < kSliceWidth; x++) {
            const uint16_t px = (y < kSwatchRows) ? kOpaqueRed : kPackerFill;
            v.push_back((uint8_t)(px & 0xFFU));
            v.push_back((uint8_t)(px >> 8));
        }
    }
    return v;
}

std::vector<Scene3d::Tile> ScatterIntoOneTile(const Inz::AtlasGrid& grid) {
    Gcz::Tile parsed;
    std::string err;
    REQUIRE(Gcz::Parse(PackedSliceWithFiller(), parsed, err));

    std::vector<Scene3d::Tile> tiles(1);
    tiles[0].width = grid.tile_width;
    tiles[0].height = grid.tile_height;
    tiles[0].bgra.assign((size_t)grid.tile_width * (size_t)grid.tile_height * 4, 0);
    Scene3d::ScatterSlice(parsed, grid, 0, 0, tiles);
    return tiles;
}

}

TEST_CASE("atlas scatter color keys the packer filler to transparent black") {
    const Inz::AtlasGrid grid = {.tile_width = 16, .tile_height = 16, .tiles_per_row = 1};
    const std::vector<Scene3d::Tile> tiles = ScatterIntoOneTile(grid);
    const std::vector<uint8_t>& bgra = tiles[0].bgra;

    for (int y = kSwatchRows; y < kSliceHeight; y++) {
        for (int x = 0; x < kSliceWidth; x++) {
            const size_t at = (((size_t)y * (size_t)grid.tile_width) + (size_t)x) * 4;
            INFO("filler texel at " << x << "," << y);
            CHECK((int)bgra[at + 0] == 0);
            CHECK((int)bgra[at + 1] == 0);
            CHECK((int)bgra[at + 2] == 0);
            CHECK((int)bgra[at + 3] == 0);
        }
    }
}

TEST_CASE("atlas scatter keeps opaque swatch texels untouched") {
    const Inz::AtlasGrid grid = {.tile_width = 16, .tile_height = 16, .tiles_per_row = 1};
    const std::vector<Scene3d::Tile> tiles = ScatterIntoOneTile(grid);
    const std::vector<uint8_t>& bgra = tiles[0].bgra;

    for (int y = 0; y < kSwatchRows; y++) {
        for (int x = 0; x < kSliceWidth; x++) {
            const size_t at = (((size_t)y * (size_t)grid.tile_width) + (size_t)x) * 4;
            INFO("swatch texel at " << x << "," << y);
            CHECK((int)bgra[at + 2] > 200);
            CHECK((int)bgra[at + 1] < 60);
            CHECK((int)bgra[at + 3] == 255);
        }
    }
}
