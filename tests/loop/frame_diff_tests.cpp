#include <catch2/catch_test_macros.hpp>

#include "loop/frame_diff.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

std::vector<uint8_t> SolidBgra(int px, uint8_t g) {
    std::vector<uint8_t> v(static_cast<std::size_t>(px) * 4, 0);
    for (int i = 0; i < px; i++)
        v[(static_cast<std::size_t>(i) * 4) + 1] = g;
    return v;
}

}

TEST_CASE("FrameDiffMad is zero for identical buffers") {
    const std::vector<uint8_t> a = SolidBgra(256, 100);
    CHECK(Loop::FrameDiffMad(a, a) == 0.0);
}

TEST_CASE("FrameDiffMad measures the G-channel gap") {
    const std::vector<uint8_t> a = SolidBgra(256, 100);
    const std::vector<uint8_t> b = SolidBgra(256, 130);
    CHECK(Loop::FrameDiffMad(a, b) == 30.0);
}

TEST_CASE("FrameDiffMad ignores B and R channels") {
    const std::vector<uint8_t> a = SolidBgra(64, 50);
    std::vector<uint8_t> b = SolidBgra(64, 50);
    for (std::size_t i = 0; i < b.size(); i += 4) {
        b[i] = 200;
        b[i + 2] = 200;
    }
    CHECK(Loop::FrameDiffMad(a, b) == 0.0);
}

TEST_CASE("FrameDiffMad samples every 16th pixel") {
    const std::vector<uint8_t> a = SolidBgra(64, 0);
    std::vector<uint8_t> b = SolidBgra(64, 0);
    b[(1 * 4) + 1] = 255;
    CHECK(Loop::FrameDiffMad(a, b) == 0.0);
    b[(16 * 4) + 1] = 255;
    CHECK(Loop::FrameDiffMad(a, b) == 255.0 / 4.0);
}

TEST_CASE("FrameDiffMad returns the mismatch sentinel for bad input") {
    const std::vector<uint8_t> a = SolidBgra(64, 0);
    const std::vector<uint8_t> b = SolidBgra(32, 0);
    CHECK(Loop::FrameDiffMad(a, b) == 1e9);
    const std::vector<uint8_t> empty;
    CHECK(Loop::FrameDiffMad(empty, empty) == 1e9);
}
