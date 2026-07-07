#include <catch2/catch_test_macros.hpp>

#include "formats/bgra_crop.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

std::vector<uint8_t> Ramp(int aw, int ah) {
    std::vector<uint8_t> v(static_cast<std::size_t>(aw) * ah * 4);
    for (std::size_t i = 0; i < v.size(); ++i)
        v[i] = static_cast<uint8_t>(i);
    return v;
}

}

TEST_CASE("Bgra::Crop copies an in-bounds rectangle") {
    const std::vector<uint8_t> atlas = Ramp(4, 2);
    int x = 1;
    int y = 0;
    int w = 2;
    int h = 1;
    const std::vector<uint8_t> out = Bgra::Crop(atlas, 4, 2, x, y, w, h);
    CHECK(out.size() == 8);
    CHECK(out[0] == 4);
    CHECK(out[7] == 11);
    CHECK(x == 1);
    CHECK(w == 2);
}

TEST_CASE("Bgra::Crop clamps a negative origin and shrinks the size") {
    const std::vector<uint8_t> atlas = Ramp(4, 2);
    int x = -1;
    int y = 0;
    int w = 3;
    int h = 1;
    const std::vector<uint8_t> out = Bgra::Crop(atlas, 4, 2, x, y, w, h);
    CHECK(x == 0);
    CHECK(w == 2);
    CHECK(out.size() == 8);
    CHECK(out[0] == 0);
}

TEST_CASE("Bgra::Crop clamps a rectangle that overflows the atlas") {
    const std::vector<uint8_t> atlas = Ramp(4, 2);
    int x = 3;
    int y = 0;
    int w = 3;
    int h = 1;
    const std::vector<uint8_t> out = Bgra::Crop(atlas, 4, 2, x, y, w, h);
    CHECK(w == 1);
    CHECK(out.size() == 4);
    CHECK(out[0] == 12);
}

TEST_CASE("Bgra::Crop returns empty for a fully out-of-bounds rectangle") {
    const std::vector<uint8_t> atlas = Ramp(4, 2);
    int x = 10;
    int y = 0;
    int w = 2;
    int h = 1;
    const std::vector<uint8_t> out = Bgra::Crop(atlas, 4, 2, x, y, w, h);
    CHECK(out.empty());
    CHECK(w == 0);
    CHECK(h == 0);
}
