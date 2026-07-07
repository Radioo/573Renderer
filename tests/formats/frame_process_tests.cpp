#include <catch2/catch_test_macros.hpp>

#include "formats/frame_process.h"

#include <cstddef>
#include <cstdint>
#include <vector>

TEST_CASE("CompositeOverOpaqueBg matches the premultiplied source-over formula") {
    std::vector<uint8_t> px = {128, 128, 128, 128};
    Frame::CompositeOverOpaqueBg(px, 1.0F, 1.0F, 1.0F);
    CHECK((int)px[0] == 128 + (((255 * 127) + 127) / 255));
    CHECK((int)px[3] == 255);

    std::vector<uint8_t> additive = {100, 100, 100, 0};
    Frame::CompositeOverOpaqueBg(additive, 0.5F, 0.5F, 0.5F);
    CHECK((int)additive[0] == 100 + (((128 * 255) + 127) / 255));
    CHECK((int)additive[3] == 255);

    std::vector<uint8_t> saturate = {250, 250, 250, 0};
    Frame::CompositeOverOpaqueBg(saturate, 1.0F, 1.0F, 1.0F);
    CHECK((int)saturate[0] == 255);
}

TEST_CASE("ClampCropToImage keeps in-range crops and clamps degenerate ones to 1x1") {
    const Frame::CropSpec in =
        Frame::ClampCropToImage({.x = 10, .y = 20, .w = 30, .h = 40}, 100, 100);
    CHECK(in.x == 10);
    CHECK(in.w == 30);

    const Frame::CropSpec neg =
        Frame::ClampCropToImage({.x = -5, .y = -5, .w = 10, .h = 10}, 100, 100);
    CHECK(neg.x == 0);
    CHECK(neg.y == 0);
    CHECK(neg.w == 10);

    const Frame::CropSpec over =
        Frame::ClampCropToImage({.x = 95, .y = 95, .w = 30, .h = 30}, 100, 100);
    CHECK(over.x == 95);
    CHECK(over.w == 5);

    const Frame::CropSpec outside =
        Frame::ClampCropToImage({.x = 500, .y = 500, .w = 10, .h = 10}, 100, 100);
    CHECK(outside.x == 99);
    CHECK(outside.y == 99);
    CHECK(outside.w == 1);
    CHECK(outside.h == 1);
}

TEST_CASE("CopyCropRegion extracts the addressed pixels row by row") {
    std::vector<uint8_t> img(std::size_t{4} * 4 * 4, 0);
    const auto set = [&](int x, int y, uint8_t v) { img[(((std::size_t)y * 4) + x) * 4] = v; };
    set(1, 1, 11);
    set(2, 1, 21);
    set(1, 2, 12);
    set(2, 2, 22);

    std::vector<uint8_t> out;
    Frame::CopyCropRegion(img, 4, {.x = 1, .y = 1, .w = 2, .h = 2}, out);
    REQUIRE(out.size() == std::size_t{2} * 2 * 4);
    CHECK((int)out[0] == 11);
    CHECK((int)out[4] == 21);
    CHECK((int)out[8] == 12);
    CHECK((int)out[12] == 22);
}
