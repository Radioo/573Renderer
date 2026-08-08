#include <catch2/catch_test_macros.hpp>

#include "formats/txp2.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

void PushU32Be(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(static_cast<uint8_t>((x >> 24) & 0xFFU));
    v.push_back(static_cast<uint8_t>((x >> 16) & 0xFFU));
    v.push_back(static_cast<uint8_t>((x >> 8) & 0xFFU));
    v.push_back(static_cast<uint8_t>(x & 0xFFU));
}

void PushU16Be(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(static_cast<uint8_t>((x >> 8) & 0xFFU));
    v.push_back(static_cast<uint8_t>(x & 0xFFU));
}

constexpr uint32_t kFlagTextures = 0x0001U;
constexpr uint32_t kFlagCells = 0x0008U;
constexpr uint32_t kFlagCoreSize = 0x0400U;
constexpr uint32_t kFlagAfpStreams = 0x0800U;

std::vector<uint8_t> BuildMinimalPackage() {
    const uint32_t flags = kFlagTextures | kFlagCells | kFlagCoreSize | kFlagAfpStreams;
    const uint32_t header_size = 24 + ((2 + 2 + 1 + 2) * 4);

    std::vector<uint8_t> v;
    v.insert(v.end(), {'T', 'X', 'P', '2'});
    PushU32Be(v, 0);
    PushU32Be(v, 0);
    PushU32Be(v, 0);
    PushU32Be(v, header_size);
    PushU32Be(v, flags);

    const uint32_t tex_array = header_size;
    const uint32_t cell_array = tex_array + 12;
    const uint32_t afp_array = cell_array + 10;
    const uint32_t names_at = afp_array + 12;

    PushU32Be(v, 1);
    PushU32Be(v, tex_array);
    PushU32Be(v, 1);
    PushU32Be(v, cell_array);
    PushU32Be(v, 0);
    PushU32Be(v, 1);
    PushU32Be(v, afp_array);
    REQUIRE(v.size() == header_size);

    const uint32_t tex_name_off = names_at;
    const uint32_t afp_name_off = names_at + 5;

    PushU32Be(v, tex_name_off);
    PushU32Be(v, 0x40);
    PushU32Be(v, 0x1000);

    PushU16Be(v, 3);
    PushU16Be(v, 10);
    PushU16Be(v, 20);
    PushU16Be(v, 30);
    PushU16Be(v, 40);

    PushU32Be(v, afp_name_off);
    PushU32Be(v, 0x80);
    PushU32Be(v, 0x2000);

    REQUIRE(v.size() == names_at);
    for (char c : std::string("tex0"))
        v.push_back(static_cast<uint8_t>(c));
    v.push_back(0);
    for (char c : std::string("anim"))
        v.push_back(static_cast<uint8_t>(c));
    v.push_back(0);
    return v;
}

}

namespace {

constexpr uint32_t kFlagGeometry = 0x2000U;

void PushF32Be(std::vector<uint8_t>& v, float f) {
    uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof(bits));
    PushU32Be(v, bits);
}

void PushGeoVertexData(std::vector<uint8_t>& v) {
    PushF32Be(v, 0.0F);
    PushF32Be(v, 0.0F);
    PushF32Be(v, 10.0F);
    PushF32Be(v, 0.0F);
    PushF32Be(v, 10.0F);
    PushF32Be(v, 20.0F);
    for (int i = 0; i < 6; i++)
        PushF32Be(v, 0.5F);
}

void PushGeoPrimNode(std::vector<uint8_t>& v, uint32_t index_offset) {
    v.push_back(0);
    v.push_back(0x0B);
    v.push_back(0);
    v.push_back(0);
    PushU16Be(v, 3);
    PushU16Be(v, 0);
    v.insert(v.end(), {0x10, 0x20, 0x30, 0x40});
    PushU32Be(v, index_offset);
}

void PushGeoHeaderAndBodyCounts(std::vector<uint8_t>& v, uint32_t header_size, uint32_t geo_array,
                                uint32_t name_at, uint32_t body) {
    v.insert(v.end(), {'T', 'X', 'P', '2'});
    for (int i = 0; i < 3; i++)
        PushU32Be(v, 0);
    PushU32Be(v, header_size);
    PushU32Be(v, kFlagGeometry);

    PushU32Be(v, 1);
    PushU32Be(v, geo_array);
    REQUIRE(v.size() == header_size);

    PushU32Be(v, name_at);
    PushU32Be(v, 0);
    PushU32Be(v, body);

    for (char c : std::string("shape0"))
        v.push_back(static_cast<uint8_t>(c));
    v.push_back(0);
    v.push_back(0);
    REQUIRE(v.size() == body);

    for (int i = 0; i < 20; i++)
        v.push_back(0);
    PushU16Be(v, 3);
    PushU16Be(v, 3);
    PushU16Be(v, 0);
    PushU16Be(v, 1);
    PushU16Be(v, 1);
    v.push_back(0);
    v.push_back(0);
}

std::vector<uint8_t> BuildGeometryPackage() {
    const uint32_t header_size = 24 + (2 * 4);
    const uint32_t geo_array = header_size;
    const uint32_t name_at = geo_array + 12;
    const uint32_t body = name_at + 8;

    std::vector<uint8_t> v;
    PushGeoHeaderAndBodyCounts(v, header_size, geo_array, name_at, body);

    const uint32_t positions = 52;
    const uint32_t uvs = positions + 24;
    const uint32_t refs = uvs + 24;
    const uint32_t prims = refs + 4;
    const uint32_t indices = prims + 16;
    const uint32_t ref_name = indices + 6;

    PushU32Be(v, positions);
    PushU32Be(v, uvs);
    PushU32Be(v, 0);
    PushU32Be(v, refs);
    PushU32Be(v, prims);
    REQUIRE(v.size() == body + positions);

    PushGeoVertexData(v);

    PushU32Be(v, ref_name);
    PushGeoPrimNode(v, indices);

    PushU16Be(v, 0);
    PushU16Be(v, 1);
    PushU16Be(v, 2);

    for (char c : std::string("cell0"))
        v.push_back(static_cast<uint8_t>(c));
    v.push_back(0);
    return v;
}

}

TEST_CASE("txp2 parses the geometry section, its primitives and bitmap refs") {
    const std::vector<uint8_t> data = BuildGeometryPackage();
    Txp2::Package pkg;
    std::string err;
    REQUIRE(Txp2::Parse(data, pkg, err));

    REQUIRE(pkg.shapes.size() == 1);
    const auto& shape = pkg.shapes[0];
    REQUIRE(shape.name == "shape0");
    REQUIRE(shape.vertex_count == 3);
    REQUIRE(shape.positions.size() == 6);
    REQUIRE(shape.positions[2] == 10.0F);
    REQUIRE(shape.positions[5] == 20.0F);
    REQUIRE(shape.uvs.size() == 6);
    REQUIRE(shape.bitmap_names.size() == 1);
    REQUIRE(shape.bitmap_names[0] == "cell0");

    REQUIRE(shape.prims.size() == 1);
    const auto& prim = shape.prims[0];
    REQUIRE(prim.flags == 0x0B);
    REQUIRE(prim.bitmap_ref == 0);
    REQUIRE(prim.index_count == 3);
    REQUIRE(prim.rgba[0] == 0x10);
    REQUIRE(prim.rgba[3] == 0x40);
    REQUIRE(prim.indices == std::vector<uint16_t>{0, 1, 2});
}

TEST_CASE("txp2 rejects a buffer that is not a package") {
    std::vector<uint8_t> const junk(64, 0xAB);
    Txp2::Package pkg;
    std::string err;
    REQUIRE_FALSE(Txp2::Parse(junk, pkg, err));
    REQUIRE(err.find("not TXP2") != std::string::npos);
}

TEST_CASE("txp2 parses the big-endian header and its sections") {
    const std::vector<uint8_t> data = BuildMinimalPackage();
    Txp2::Package pkg;
    std::string err;
    REQUIRE(Txp2::Parse(data, pkg, err));
    REQUIRE(err.empty());

    REQUIRE(pkg.big_endian);
    REQUIRE(pkg.core_size == 0);

    REQUIRE(pkg.afp_entries.size() == 1);
    REQUIRE(pkg.afp_entries[0].name == "anim");
    REQUIRE(pkg.afp_entries[0].size == 0x80);
    REQUIRE(pkg.afp_entries[0].data_offset == 0x2000);

    REQUIRE(pkg.textures.size() == 1);
    REQUIRE(pkg.textures[0].name == "tex0");
    REQUIRE(pkg.textures[0].size == 0x40);
    REQUIRE(pkg.textures[0].file_offset == 0x1000);

    REQUIRE(pkg.cells.size() == 1);
    REQUIRE(pkg.cells[0].texture_index == 3);
    REQUIRE(pkg.cells[0].x0 == 10);
    REQUIRE(pkg.cells[0].y1 == 40);
}

TEST_CASE("txp2 cell names carry the stored cell index, not their table position") {
    std::vector<uint8_t> v;
    v.insert(v.end(), {'T', 'X', 'P', '2'});
    for (int i = 0; i < 3; i++)
        PushU32Be(v, 0);
    const uint32_t header_size = 24 + ((2 + 1) * 4);
    PushU32Be(v, header_size);
    PushU32Be(v, kFlagCells | 0x10U);

    const uint32_t cell_array = header_size;
    const uint32_t name_obj = cell_array + 30;
    const uint32_t name_arr = name_obj + 28;
    const uint32_t names_at = name_arr + 24;

    PushU32Be(v, 3);
    PushU32Be(v, cell_array);
    PushU32Be(v, name_obj);
    REQUIRE(v.size() == header_size);

    for (uint16_t i = 0; i < 3; i++) {
        PushU16Be(v, i);
        PushU16Be(v, static_cast<uint16_t>(10 * (i + 1)));
        PushU16Be(v, 0);
        PushU16Be(v, 0);
        PushU16Be(v, 0);
    }
    REQUIRE(v.size() == name_obj);

    for (int i = 0; i < 4; i++)
        PushU32Be(v, 0);
    PushU32Be(v, 2);
    PushU32Be(v, 0);
    PushU32Be(v, name_arr);
    REQUIRE(v.size() == name_arr);

    PushU32Be(v, 0);
    PushU32Be(v, 2);
    PushU32Be(v, names_at);
    PushU32Be(v, 0);
    PushU32Be(v, 0);
    PushU32Be(v, names_at + 6);
    REQUIRE(v.size() == names_at);

    for (char c : std::string("later"))
        v.push_back(static_cast<uint8_t>(c));
    v.push_back(0);
    for (char c : std::string("first"))
        v.push_back(static_cast<uint8_t>(c));
    v.push_back(0);

    Txp2::Package pkg;
    std::string err;
    REQUIRE(Txp2::Parse(v, pkg, err));

    REQUIRE(pkg.cell_names.size() == 2);
    REQUIRE(pkg.cell_names[0].name == "later");
    REQUIRE(pkg.cell_names[0].cell_index == 2);
    REQUIRE(pkg.cell_names[1].name == "first");
    REQUIRE(pkg.cell_names[1].cell_index == 0);

    REQUIRE(pkg.cells[pkg.cell_names[0].cell_index].x0 == 30);
    REQUIRE(pkg.cells[pkg.cell_names[1].cell_index].x0 == 10);
}

TEST_CASE("txp2 rejects a header whose size disagrees with its flag word") {
    std::vector<uint8_t> data = BuildMinimalPackage();
    data[16] = 0;
    data[17] = 0;
    data[18] = 0;
    data[19] = 0x70;
    Txp2::Package pkg;
    std::string err;
    REQUIRE_FALSE(Txp2::Parse(data, pkg, err));
    REQUIRE(err.find("disagrees") != std::string::npos);
}

TEST_CASE("txp2 section dword counts match the packed header layout") {
    REQUIRE(Txp2::SectionDwordCount(0x0001U) == 2);
    REQUIRE(Txp2::SectionDwordCount(0x0002U) == 1);
    REQUIRE(Txp2::SectionDwordCount(0x0400U) == 1);
    REQUIRE(Txp2::SectionDwordCount(0x0800U) == 2);
    REQUIRE(Txp2::SectionDwordCount(0x2000U) == 2);

    uint32_t total = 0;
    for (const uint32_t bit : {0x1U, 0x2U, 0x8U, 0x10U, 0x40U, 0x80U, 0x100U, 0x200U, 0x400U,
                               0x800U, 0x1000U, 0x2000U, 0x4000U, 0x8000U, 0x10000U, 0x20000U}) {
        total += Txp2::SectionDwordCount(bit);
    }
    REQUIRE(total == 22);
}
