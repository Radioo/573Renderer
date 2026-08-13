#include <catch2/catch_test_macros.hpp>

#include "render/stretch.h"

#include <string>

TEST_CASE("four-three detection only accepts landscape 4:3") {
    REQUIRE(Stretch::IsFourThree(640, 480));
    REQUIRE(Stretch::IsFourThree(800, 600));
    REQUIRE(Stretch::IsFourThree(1280, 960));
    REQUIRE_FALSE(Stretch::IsFourThree(1280, 720));
    REQUIRE_FALSE(Stretch::IsFourThree(1920, 1080));
    REQUIRE_FALSE(Stretch::IsFourThree(480, 640));
    REQUIRE_FALSE(Stretch::IsFourThree(1080, 1920));
    REQUIRE_FALSE(Stretch::IsFourThree(520, 704));
    REQUIRE_FALSE(Stretch::IsFourThree(0, 0));
    REQUIRE_FALSE(Stretch::IsFourThree(-4, -3));
}

TEST_CASE("present size widens 4:3 to 16:9 and keeps the height") {
    REQUIRE(Stretch::Present(640, 480, true) == Stretch::Size{.w = 854, .h = 480});
    REQUIRE(Stretch::Present(800, 600, true) == Stretch::Size{.w = 1068, .h = 600});
    REQUIRE(Stretch::Present(1280, 960, true) == Stretch::Size{.w = 1708, .h = 960});
}

TEST_CASE("present width is always even so video encoders accept it") {
    for (int h = 120; h <= 2160; h += 3) {
        const int w = h * 4 / 3;
        if (!Stretch::IsFourThree(w, h)) continue;
        const Stretch::Size size = Stretch::Present(w, h, true);
        REQUIRE(size.w % 2 == 0);
        REQUIRE(size.h == h);
        REQUIRE(size.w > w);
    }
}

TEST_CASE("present is a no-op when stretching is off or the source is not 4:3") {
    REQUIRE(Stretch::Present(640, 480, false) == Stretch::Size{.w = 640, .h = 480});
    REQUIRE(Stretch::Present(1280, 720, true) == Stretch::Size{.w = 1280, .h = 720});
    REQUIRE(Stretch::Present(1080, 1920, true) == Stretch::Size{.w = 1080, .h = 1920});
}

TEST_CASE("every filter has a distinct name") {
    std::string joined;
    int count = 0;
    for (const Stretch::Filter filter : Stretch::AllFilters()) {
        const std::string name = Stretch::FilterName(filter);
        REQUIRE_FALSE(name.empty());
        REQUIRE(joined.find(name) == std::string::npos);
        joined += name + ";";
        count++;
    }
    REQUIRE(count == 4);
}
