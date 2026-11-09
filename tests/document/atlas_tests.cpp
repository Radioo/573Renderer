#include <catch2/catch_test_macros.hpp>

#include "document/atlas.h"
#include "document/atlas_write.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

Document::AtlasCell Cell(const std::string& name, uint32_t width, uint32_t height) {
    return Document::AtlasCell{.name = name, .width = width, .height = height};
}

bool Overlap(const Document::AtlasPlacement& a, const Document::AtlasPlacement& b) {
    return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height &&
           b.y < a.y + a.height;
}

void CheckLaidOut(const Document::Atlas& atlas) {
    for (std::size_t i = 0; i < atlas.images.size(); i++) {
        const Document::AtlasPlacement& one = atlas.images[i];
        CHECK(one.x + one.width <= atlas.width);
        CHECK(one.y + one.height <= atlas.height);
        for (std::size_t j = i + 1; j < atlas.images.size(); j++)
            CHECK_FALSE(Overlap(one, atlas.images[j]));
    }
}

bool PowerOfTwo(uint32_t value) {
    return value > 0 && (value & (value - 1)) == 0;
}

}

TEST_CASE("A packed atlas holds every image without overlap") {
    const std::vector<Document::AtlasCell> cells{Cell("a", 40, 40), Cell("b", 100, 20),
                                                 Cell("c", 20, 100), Cell("d", 7, 9)};
    const auto atlas = Document::PackAtlas(cells);
    if (!atlas) FAIL(atlas.error());
    CHECK(atlas->images.size() == cells.size());
    CHECK(PowerOfTwo(atlas->width));
    CHECK(PowerOfTwo(atlas->height));
    CheckLaidOut(*atlas);
}

TEST_CASE("Packing the same images twice gives the same atlas") {
    const std::vector<Document::AtlasCell> cells{Cell("a", 40, 40), Cell("b", 100, 20),
                                                 Cell("c", 20, 100)};
    const auto first = Document::PackAtlas(cells);
    const auto second = Document::PackAtlas(cells);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(*first == *second);
}

TEST_CASE("The order the images are given in does not change the atlas") {
    const std::vector<Document::AtlasCell> one{Cell("a", 40, 40), Cell("b", 40, 40),
                                               Cell("c", 40, 40)};
    const std::vector<Document::AtlasCell> other{Cell("c", 40, 40), Cell("a", 40, 40),
                                                 Cell("b", 40, 40)};
    const auto first = Document::PackAtlas(one);
    const auto second = Document::PackAtlas(other);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(*first == *second);
}

TEST_CASE("An atlas grows only as far as it has to") {
    const auto small = Document::PackAtlas(std::vector<Document::AtlasCell>{Cell("a", 8, 8)});
    REQUIRE(small.has_value());
    CHECK(small->width == 64);
    CHECK(small->height == 64);

    const auto taller = Document::PackAtlas(std::vector<Document::AtlasCell>{Cell("a", 64, 200)});
    REQUIRE(taller.has_value());
    CHECK(taller->width == 64);
    CHECK(taller->height == 256);
    CheckLaidOut(*taller);
}

TEST_CASE("An atlas that cannot hold its images says so") {
    CHECK_FALSE(Document::PackAtlas({}).has_value());
    CHECK_FALSE(
        Document::PackAtlas(std::vector<Document::AtlasCell>{Cell("a", 0, 10)}).has_value());
    CHECK_FALSE(
        Document::PackAtlas(std::vector<Document::AtlasCell>{Cell("a", 9000, 10)}).has_value());
    CHECK_FALSE(
        Document::PackAtlas(std::vector<Document::AtlasCell>{Cell("a", 10, 10), Cell("a", 20, 20)})
            .has_value());

    std::vector<Document::AtlasCell> many;
    many.reserve(80);
    for (int i = 0; i < 80; i++)
        many.push_back(Cell("image" + std::to_string(i), 1000, 1000));
    CHECK_FALSE(Document::PackAtlas(many).has_value());
}

TEST_CASE("A guard ring repeats the edge pixels around the image") {
    Document::LoadedImage image{.width = 2, .height = 2, .bgra = {}};
    image.bgra = {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4};

    const Document::LoadedImage ringed = Document::WithGuardRing(image);
    CHECK(ringed.width == 4);
    CHECK(ringed.height == 4);
    REQUIRE(ringed.bgra.size() == std::size_t{4} * 4 * 4);

    const std::vector<uint8_t> expected{1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
                                        1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
                                        3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
                                        3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};
    CHECK(ringed.bgra == expected);
}

TEST_CASE("A guard ring on a one pixel image repeats that pixel everywhere") {
    const Document::LoadedImage image{.width = 1, .height = 1, .bgra = {9, 8, 7, 6}};
    const Document::LoadedImage ringed = Document::WithGuardRing(image);
    CHECK(ringed.width == 3);
    CHECK(ringed.height == 3);
    REQUIRE(ringed.bgra.size() == std::size_t{3} * 3 * 4);
    for (std::size_t i = 0; i < ringed.bgra.size(); i += 4) {
        CHECK(ringed.bgra[i] == 9);
        CHECK(ringed.bgra[i + 3] == 6);
    }
}
