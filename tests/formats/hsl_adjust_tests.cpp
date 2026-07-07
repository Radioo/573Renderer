#include <catch2/catch_test_macros.hpp>

#include "formats/hsl_adjust.h"

#include <cstdint>

TEST_CASE("zero deltas preserve exact primaries and greys") {
    CHECK(Hsl::AdjustArgb(0xFFFFFFFF, 0.0F, 0.0F, 0.0F) == 0xFFFFFFFF);
    CHECK(Hsl::AdjustArgb(0xFF000000, 0.0F, 0.0F, 0.0F) == 0xFF000000);
    CHECK(Hsl::AdjustArgb(0xFF808080, 0.0F, 0.0F, 0.0F) == 0xFF808080);
    CHECK(Hsl::AdjustArgb(0xFFFF0000, 0.0F, 0.0F, 0.0F) == 0xFFFF0000);
}

TEST_CASE("bg_0009 real game ground truth: white through the line filter") {
    const uint32_t out =
        Hsl::AdjustArgb(0xFFFFFFFF, -136.0F / 360.0F, 38.0F / 100.0F, -69.0F / 100.0F);
    CHECK(out == 0xFF31416D);
}

TEST_CASE("alpha channel passes through untouched") {
    const uint32_t out =
        Hsl::AdjustArgb(0x80FFFFFF, -136.0F / 360.0F, 38.0F / 100.0F, -69.0F / 100.0F);
    CHECK(out == 0x8031416D);
}

TEST_CASE("hue offset wraps modulo a full turn") {
    CHECK(Hsl::AdjustArgb(0xFFFF0000, 1.0F, 0.0F, 0.0F) == 0xFFFF0000);
    CHECK(Hsl::AdjustArgb(0xFFFF0000, -1.0F, 0.0F, 0.0F) == 0xFFFF0000);
}

TEST_CASE("lightness delta drives toward black and white") {
    CHECK(Hsl::AdjustArgb(0xFF808080, 0.0F, 0.0F, 1.0F) == 0xFFFFFFFF);
    CHECK(Hsl::AdjustArgb(0xFF808080, 0.0F, 0.0F, -1.0F) == 0xFF000000);
}
