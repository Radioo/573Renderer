#include <catch2/catch_test_macros.hpp>

#include "editor_thumbnail.h"

#include <QColor>
#include <QImage>
#include <QSize>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

constexpr int kSide = 32;
constexpr uint8_t kBlue = 0xC0;
constexpr uint8_t kRed = 0x20;

std::vector<uint8_t> Filled(int side, uint8_t blue, uint8_t red) {
    std::vector<uint8_t> bgra(static_cast<std::size_t>(side) * side * 4);
    for (std::size_t at = 0; at + 3 < bgra.size(); at += 4) {
        bgra[at] = blue;
        bgra[at + 1] = 0x40;
        bgra[at + 2] = red;
        bgra[at + 3] = 0xFF;
    }
    return bgra;
}

}

TEST_CASE("A thumbnail keeps its pixels when the buffer it was read from is gone") {
    std::vector<uint8_t> pixels = Filled(kSide, kBlue, kRed);
    const QImage tile = Editor::Thumbnail(pixels, kSide, kSide, QSize(kSide, kSide));
    REQUIRE_FALSE(tile.isNull());
    const QColor before = tile.pixelColor(0, 0);

    std::ranges::fill(pixels, uint8_t{0});

    CHECK(tile.pixelColor(0, 0) == before);
    CHECK(tile.pixelColor(0, 0).blue() == kBlue);
    CHECK(tile.pixelColor(0, 0).red() == kRed);
}

TEST_CASE("A thumbnail scales down to the size it was asked for") {
    const std::vector<uint8_t> pixels = Filled(kSide, kBlue, kRed);

    const QImage tile = Editor::Thumbnail(pixels, kSide, kSide, QSize(16, 16));

    CHECK(tile.width() == 16);
    CHECK(tile.height() == 16);
}

TEST_CASE("A thumbnail of nothing is nothing rather than a read off the end") {
    const std::vector<uint8_t> pixels = Filled(2, kBlue, kRed);

    CHECK(Editor::Thumbnail(pixels, 0, 0, QSize(kSide, kSide)).isNull());
    CHECK(Editor::Thumbnail(pixels, kSide, kSide, QSize(kSide, kSide)).isNull());
    CHECK(Editor::Thumbnail(pixels, 2, 2, QSize(0, 0)).isNull());
}
