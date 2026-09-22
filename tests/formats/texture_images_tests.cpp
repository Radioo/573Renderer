#include <catch2/catch_test_macros.hpp>

#include "formats/avs_lz77.h"
#include "binary_test_support.h"
#include "formats/binary_xml.h"
#include "formats/texture_images.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<uint8_t> U16s(std::initializer_list<uint16_t> values) {
    std::vector<uint8_t> out;
    for (const uint16_t v : values) {
        out.push_back(static_cast<uint8_t>(v >> 8U));
        out.push_back(static_cast<uint8_t>(v & 0xFFU));
    }
    return out;
}

std::vector<uint8_t> Header(uint32_t uncompressed, uint32_t compressed) {
    std::vector<uint8_t> out;
    for (const uint32_t v : {uncompressed, compressed}) {
        for (int shift = 24; shift >= 0; shift -= 8) {
            out.push_back(static_cast<uint8_t>((v >> static_cast<uint32_t>(shift)) & 0xFFU));
        }
    }
    return out;
}

std::vector<uint8_t> Pixels(std::size_t count) {
    std::vector<uint8_t> out(count * 4);
    for (std::size_t i = 0; i < out.size(); i++) {
        out[i] = static_cast<uint8_t>((i % 7) * 31U);
    }
    return out;
}

BinaryXml::Document TextureList(bool compressed) {
    constexpr uint8_t k2U16 = 19;
    using BinaryXml::Type::k4U16;
    using BinaryXml::Type::kVoid;
    BinaryXml::Document doc;
    doc.root = TestSupport::MakeNode(kVoid, "texturelist");
    if (compressed) {
        doc.root.attributes.push_back(TestSupport::MakeNode(BinaryXml::Type::kAttribute, "compress",
                                                            TestSupport::TextBytes("avslz")));
    }
    BinaryXml::Node texture = TestSupport::MakeNode(kVoid, "texture");
    texture.attributes.push_back(TestSupport::MakeNode(BinaryXml::Type::kAttribute, "format",
                                                       TestSupport::TextBytes("argb8888rev")));
    texture.attributes.push_back(TestSupport::MakeNode(BinaryXml::Type::kAttribute, "name",
                                                       TestSupport::TextBytes("tex000")));
    texture.children.push_back(TestSupport::MakeNode(k2U16, "size", U16s({2048, 1024})));
    BinaryXml::Node image = TestSupport::MakeNode(kVoid, "image");
    image.attributes.push_back(
        TestSupport::MakeNode(BinaryXml::Type::kAttribute, "name", TestSupport::TextBytes("bg03")));
    image.children.push_back(TestSupport::MakeNode(k4U16, "uvrect", U16s({2, 1642, 2, 1230})));
    image.children.push_back(TestSupport::MakeNode(k4U16, "imgrect", U16s({0, 1644, 0, 1232})));
    texture.children.push_back(std::move(image));
    doc.root.children.push_back(std::move(texture));
    return doc;
}

}

TEST_CASE("ReadList reads images, their formats and pixel sizes") {
    const auto list = TextureImages::ReadList(TextureList(true));
    REQUIRE(list.has_value());
    CHECK(list->compressed);
    REQUIRE(list->images.size() == 1);
    CHECK(list->images[0].name == "bg03");
    CHECK(list->images[0].format == "argb8888rev");
    CHECK(list->images[0].width == 822);
    CHECK(list->images[0].height == 616);
    CHECK_FALSE(TextureImages::ReadList(TextureList(false))->compressed);
}

TEST_CASE("ReadList rejects an image without an imgrect") {
    BinaryXml::Document doc = TextureList(true);
    doc.root.children[0].children[1].children.pop_back();
    CHECK_FALSE(TextureImages::ReadList(doc).has_value());
}

TEST_CASE("An LZ77 blob decodes and encodes back to the same bytes") {
    const std::vector<uint8_t> pixels = Pixels(64);
    const std::vector<uint8_t> packed = AvsLz77::Compress(pixels);
    std::vector<uint8_t> blob =
        Header(static_cast<uint32_t>(pixels.size()), static_cast<uint32_t>(packed.size()));
    blob.insert(blob.end(), packed.begin(), packed.end());
    const auto decoded = TextureImages::DecodeBlob(blob, true);
    REQUIRE(decoded.has_value());
    CHECK(decoded->storage == TextureImages::Storage::Lz77);
    CHECK(decoded->pixels == pixels);
    CHECK(TextureImages::EncodeBlob(*decoded) == blob);
}

TEST_CASE("A zero compressed size means raw pixels follow the header") {
    const std::vector<uint8_t> pixels = Pixels(9);
    std::vector<uint8_t> blob = Header(static_cast<uint32_t>(pixels.size()), 0);
    blob.insert(blob.end(), pixels.begin(), pixels.end());
    const auto decoded = TextureImages::DecodeBlob(blob, true);
    REQUIRE(decoded.has_value());
    CHECK(decoded->storage == TextureImages::Storage::RawAfterHeader);
    CHECK(decoded->pixels == pixels);
    CHECK(TextureImages::EncodeBlob(*decoded) == blob);
}

TEST_CASE("Images of an uncompressed list are the pixels themselves") {
    const std::vector<uint8_t> pixels = Pixels(5);
    const auto decoded = TextureImages::DecodeBlob(pixels, false);
    REQUIRE(decoded.has_value());
    CHECK(decoded->storage == TextureImages::Storage::Plain);
    CHECK(decoded->pixels == pixels);
    CHECK(TextureImages::EncodeBlob(*decoded) == pixels);
}

TEST_CASE("DecodeBlob rejects blobs whose header disagrees with their content") {
    const std::vector<uint8_t> pixels = Pixels(16);
    const std::vector<uint8_t> packed = AvsLz77::Compress(pixels);

    std::vector<uint8_t> overrun = Header(64, static_cast<uint32_t>(packed.size() + 10));
    overrun.insert(overrun.end(), packed.begin(), packed.end());
    CHECK_FALSE(TextureImages::DecodeBlob(overrun, true).has_value());

    std::vector<uint8_t> wrong_size = Header(63, static_cast<uint32_t>(packed.size()));
    wrong_size.insert(wrong_size.end(), packed.begin(), packed.end());
    CHECK_FALSE(TextureImages::DecodeBlob(wrong_size, true).has_value());

    const std::vector<uint8_t> tiny = {0, 0, 0};
    CHECK_FALSE(TextureImages::DecodeBlob(tiny, true).has_value());
}

TEST_CASE("argb8888rev pixels are BGRA byte for byte") {
    const std::vector<uint8_t> pixels = Pixels(3);
    const auto bgra = TextureImages::PixelsToBgra("argb8888rev", pixels);
    REQUIRE(bgra.has_value());
    CHECK(*bgra == pixels);
    const auto back = TextureImages::BgraToPixels("argb8888rev", *bgra);
    REQUIRE(back.has_value());
    CHECK(*back == pixels);
}

TEST_CASE("Unsupported pixel formats are reported by name") {
    const auto bgra = TextureImages::PixelsToBgra("dxt5", Pixels(1));
    REQUIRE_FALSE(bgra.has_value());
    CHECK(bgra.error().find("dxt5") != std::string::npos);
    CHECK_FALSE(TextureImages::BgraToPixels("rgb565", Pixels(1)).has_value());
}
