#include <catch2/catch_test_macros.hpp>

#include "formats/big_endian.h"
#include "formats/ge2d_shape.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

namespace {

using Ge2dShape::ByteOrder;

constexpr uint32_t kOne = 0x3F800000;
constexpr uint32_t kHalf = 0x3F000000;
constexpr uint32_t kNegativeZero = 0x80000000;

void U32s(std::vector<uint8_t>& out, std::initializer_list<uint32_t> values) {
    for (const uint32_t v : values)
        BigEndian::AppendU32(out, v);
}

void U16s(std::vector<uint8_t>& out, std::initializer_list<uint16_t> values) {
    for (const uint16_t v : values)
        BigEndian::AppendU16(out, v);
}

std::vector<uint8_t> TexturedQuad() {
    std::vector<uint8_t> out;
    U32s(out, {0x47453244, 0x00010000, 0x00010100, 152, 0});
    U16s(out, {4, 4, 0, 1, 1, 0});
    U32s(out, {60, 92, 0, 52, 124});
    U32s(out, {56});
    out.insert(out.end(), {'a', 'b', 0, 0});
    U32s(out, {0, 0, kOne, 0, 0, kOne, kOne, kNegativeZero});
    U32s(out, {0, 0, kHalf, 0, 0, kHalf, kHalf, kHalf});
    out.insert(out.end(), {4, 0x03, 0, 0xFF});
    U16s(out, {6, 0});
    out.insert(out.end(), {0, 0, 0, 0});
    U32s(out, {140});
    U16s(out, {0, 1, 2, 2, 1, 3});
    return out;
}

std::vector<uint8_t> SolidQuadWithRect() {
    std::vector<uint8_t> out;
    U32s(out, {0x47453244, 0x00010000, 0x00010100, 128, 0x24});
    U16s(out, {4, 0, 0, 0, 1, 0});
    U32s(out, {68, 0, 0, 0, 100});
    U32s(out, {0, kOne, 0, kOne});
    U32s(out, {0, 0, kOne, 0, 0, kOne, kOne, kOne});
    out.insert(out.end(), {4, 0x09, 0xFF, 0xFF});
    U16s(out, {6, 0});
    out.insert(out.end(), {0x10, 0x20, 0x30, 0xFF});
    U32s(out, {116});
    U16s(out, {0, 1, 2, 2, 1, 3});
    return out;
}

void Reverse(std::vector<uint8_t>& data, std::size_t off, std::size_t width, std::size_t count) {
    for (std::size_t i = 0; i < count; i++) {
        std::ranges::reverse(std::span(data).subspan(off + (i * width), width));
    }
}

std::vector<uint8_t> SwappedTexturedQuad() {
    std::vector<uint8_t> data = TexturedQuad();
    Reverse(data, 0, 4, 5);
    Reverse(data, 32, 4, 5);
    Reverse(data, 20, 2, 6);
    Reverse(data, 60, 4, 8);
    Reverse(data, 92, 4, 8);
    Reverse(data, 52, 4, 1);
    Reverse(data, 124 + 4, 2, 1);
    Reverse(data, 124 + 12, 4, 1);
    Reverse(data, 140, 2, 6);
    return data;
}

}

TEST_CASE("PackageByteOrder follows the package magic file") {
    CHECK(Ge2dShape::PackageByteOrder(std::vector<uint8_t>{'N', 'G', 'P', 'F'}) == ByteOrder::Big);
    CHECK(Ge2dShape::PackageByteOrder(std::vector<uint8_t>{'F', 'P', 'G', 'N'}) ==
          ByteOrder::Little);
    CHECK(Ge2dShape::PackageByteOrder(std::vector<uint8_t>{'N', 'G', 'N', 'N'}) ==
          ByteOrder::Little);
    CHECK_FALSE(Ge2dShape::PackageByteOrder(std::vector<uint8_t>{'N', 'G', 'P'}).has_value());
}

TEST_CASE("Read a textured quad") {
    const auto shape = Ge2dShape::Read(TexturedQuad(), ByteOrder::Big);
    REQUIRE(shape.has_value());
    CHECK(shape->unread_version == 0x00010000);
    CHECK(shape->unread_value == 0x00010100);
    CHECK(shape->flags == 0);
    CHECK_FALSE(shape->rect.has_value());
    REQUIRE(shape->vertices.size() == 4);
    CHECK(shape->vertices[3] == std::array<uint32_t, 2>{kOne, kNegativeZero});
    REQUIRE(shape->uvs.size() == 4);
    CHECK(shape->uvs[1][0] == kHalf);
    CHECK(shape->vertex_colours.empty());
    CHECK(shape->texture_names == std::vector<std::string>{"ab"});
    REQUIRE(shape->primitives.size() == 1);
    const Ge2dShape::Primitive& primitive = shape->primitives[0];
    CHECK(primitive.kind == 4);
    CHECK(primitive.draw_flags == 0x03);
    CHECK(primitive.texture == 0);
    CHECK(primitive.second_texture == 0xFF);
    CHECK(primitive.indices == std::vector<uint16_t>{0, 1, 2, 2, 1, 3});
}

TEST_CASE("Write rebuilds the shipped layout byte for byte") {
    for (const std::vector<uint8_t>& original : {TexturedQuad(), SolidQuadWithRect()}) {
        const auto shape = Ge2dShape::Read(original, ByteOrder::Big);
        REQUIRE(shape.has_value());
        const auto written = Ge2dShape::Write(*shape, ByteOrder::Big);
        REQUIRE(written.has_value());
        CHECK(*written == original);
    }
}

TEST_CASE("Read keeps the rect, the flags beside it and the primitive colour") {
    const auto shape = Ge2dShape::Read(SolidQuadWithRect(), ByteOrder::Big);
    REQUIRE(shape.has_value());
    CHECK(shape->flags == 0x20);
    REQUIRE(shape->rect.has_value());
    CHECK(*shape->rect == std::array<uint32_t, 4>{0, kOne, 0, kOne});
    CHECK(shape->uvs.empty());
    CHECK(shape->texture_names.empty());
    CHECK(shape->primitives[0].colour == std::array<uint8_t, 4>{0x10, 0x20, 0x30, 0xFF});
}

TEST_CASE("Little-endian shapes swap exactly the multi-byte fields") {
    const auto big = Ge2dShape::Read(TexturedQuad(), ByteOrder::Big);
    REQUIRE(big.has_value());
    const auto little = Ge2dShape::Read(SwappedTexturedQuad(), ByteOrder::Little);
    REQUIRE(little.has_value());
    CHECK(*little == *big);
    const auto written = Ge2dShape::Write(*big, ByteOrder::Little);
    REQUIRE(written.has_value());
    CHECK(*written == SwappedTexturedQuad());
}

TEST_CASE("Primitive bytes that are never swapped read the same in both byte orders") {
    std::vector<uint8_t> big = TexturedQuad();
    big[124 + 6] = 0x12;
    big[124 + 7] = 0x34;
    std::vector<uint8_t> little = SwappedTexturedQuad();
    little[124 + 6] = 0x12;
    little[124 + 7] = 0x34;
    const auto from_big = Ge2dShape::Read(big, ByteOrder::Big);
    const auto from_little = Ge2dShape::Read(little, ByteOrder::Little);
    REQUIRE(from_big.has_value());
    REQUIRE(from_little.has_value());
    CHECK(*from_little == *from_big);
    const auto written = Ge2dShape::Write(*from_big, ByteOrder::Little);
    REQUIRE(written.has_value());
    CHECK(*written == little);
}

TEST_CASE("Write pads names and index arrays and places vertex colours after the UVs") {
    Ge2dShape::Shape shape;
    shape.vertices = {{0, 0}, {kOne, 0}, {0, kOne}};
    shape.uvs = {{0, 0}, {kOne, 0}, {0, kOne}};
    shape.vertex_colours = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};
    shape.texture_names = {"abcd"};
    shape.primitives = {Ge2dShape::Primitive{.kind = 4,
                                             .draw_flags = 0x07,
                                             .texture = 0,
                                             .second_texture = 0xFF,
                                             .unread_bytes = {},
                                             .colour = {},
                                             .indices = {0, 1, 2}}};
    const auto written = Ge2dShape::Write(shape, ByteOrder::Big);
    REQUIRE(written.has_value());
    const std::vector<uint8_t>& d = *written;
    CHECK(BigEndian::ReadU32(d, 44) == 52);
    CHECK(BigEndian::ReadU32(d, 52) == 56);
    CHECK(std::vector<uint8_t>(d.begin() + 56, d.begin() + 64) ==
          std::vector<uint8_t>{'a', 'b', 'c', 'd', 0, 0, 0, 0});
    CHECK(BigEndian::ReadU32(d, 32) == 64);
    CHECK(BigEndian::ReadU32(d, 36) == 88);
    CHECK(BigEndian::ReadU32(d, 40) == 112);
    CHECK(BigEndian::ReadU32(d, 48) == 124);
    CHECK(BigEndian::ReadU32(d, 124 + 12) == 140);
    CHECK(d.size() == 148);
    CHECK(BigEndian::ReadU32(d, 12) == 148);
    CHECK(d[146] == 0);
    CHECK(d[147] == 0);
    const auto back = Ge2dShape::Read(d, ByteOrder::Big);
    REQUIRE(back.has_value());
    CHECK(*back == shape);
}

TEST_CASE("Read rejects malformed shapes") {
    const std::vector<uint8_t> good = TexturedQuad();

    std::vector<uint8_t> bad_magic = good;
    bad_magic[0] = 'X';
    CHECK_FALSE(Ge2dShape::Read(bad_magic, ByteOrder::Big).has_value());
    CHECK_FALSE(Ge2dShape::Read(good, ByteOrder::Little).has_value());

    std::vector<uint8_t> bad_size = good;
    bad_size[15] = 151;
    CHECK_FALSE(Ge2dShape::Read(bad_size, ByteOrder::Big).has_value());

    std::vector<uint8_t> many_vertices = good;
    many_vertices[20] = 0x7F;
    CHECK_FALSE(Ge2dShape::Read(many_vertices, ByteOrder::Big).has_value());

    std::vector<uint8_t> far_indices = good;
    far_indices[124 + 14] = 0x10;
    CHECK_FALSE(Ge2dShape::Read(far_indices, ByteOrder::Big).has_value());

    std::vector<uint8_t> unterminated = good;
    unterminated[55] = 151;
    CHECK_FALSE(Ge2dShape::Read(unterminated, ByteOrder::Big).has_value());

    CHECK_FALSE(Ge2dShape::Read(std::vector<uint8_t>(40, 0), ByteOrder::Big).has_value());
}

TEST_CASE("Write rejects shapes it cannot encode") {
    auto shape = Ge2dShape::Read(TexturedQuad(), ByteOrder::Big);
    REQUIRE(shape.has_value());

    Ge2dShape::Shape nul_name = *shape;
    nul_name.texture_names[0].push_back('\0');
    CHECK_FALSE(Ge2dShape::Write(nul_name, ByteOrder::Big).has_value());

    Ge2dShape::Shape rect_flag = *shape;
    rect_flag.flags = 0x4;
    CHECK_FALSE(Ge2dShape::Write(rect_flag, ByteOrder::Big).has_value());

    Ge2dShape::Shape too_many = *shape;
    too_many.vertices.resize(0x10000);
    CHECK_FALSE(Ge2dShape::Write(too_many, ByteOrder::Big).has_value());
}

TEST_CASE("Read refuses bytes and offsets the model would drop") {
    const std::vector<uint8_t> good = TexturedQuad();

    std::vector<uint8_t> dirty_padding = good;
    dirty_padding[59] = 0x41;
    CHECK_FALSE(Ge2dShape::Read(dirty_padding, ByteOrder::Big).has_value());

    std::vector<uint8_t> stray_offset = good;
    stray_offset[43] = 60;
    CHECK_FALSE(Ge2dShape::Read(stray_offset, ByteOrder::Big).has_value());

    std::vector<uint8_t> trailing = good;
    trailing.insert(trailing.end(), {0, 0, 0, 7});
    trailing[15] = static_cast<uint8_t>(trailing.size());
    CHECK_FALSE(Ge2dShape::Read(trailing, ByteOrder::Big).has_value());

    std::vector<uint8_t> overlapping = good;
    overlapping[39] = 60;
    CHECK_FALSE(Ge2dShape::Read(overlapping, ByteOrder::Big).has_value());
}

TEST_CASE("Write gives an empty index array a zero offset") {
    auto shape = Ge2dShape::Read(SolidQuadWithRect(), ByteOrder::Big);
    REQUIRE(shape.has_value());
    shape->primitives[0].indices.clear();
    const auto written = Ge2dShape::Write(*shape, ByteOrder::Big);
    REQUIRE(written.has_value());
    CHECK(BigEndian::ReadU32(*written, 100 + 12) == 0);
    const auto back = Ge2dShape::Read(*written, ByteOrder::Big);
    REQUIRE(back.has_value());
    CHECK(*back == *shape);
}
