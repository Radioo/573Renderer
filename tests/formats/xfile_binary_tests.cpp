#include <catch2/catch_test_macros.hpp>

#include "formats/xfile.h"
#include "formats/xfile_binary.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr uint16_t kName = 1;
constexpr uint16_t kInteger = 3;
constexpr uint16_t kIntegerList = 6;
constexpr uint16_t kFloatList = 7;
constexpr uint16_t kOBrace = 10;
constexpr uint16_t kCBrace = 11;

void PushU16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back((uint8_t)(x & 0xFFU));
    v.push_back((uint8_t)(x >> 8U));
}

void PushU32(std::vector<uint8_t>& v, uint32_t x) {
    for (int i = 0; i < 4; i++)
        v.push_back((uint8_t)((x >> (8U * (unsigned)i)) & 0xFFU));
}

void PushName(std::vector<uint8_t>& v, std::string_view name) {
    PushU16(v, kName);
    PushU32(v, (uint32_t)name.size());
    for (const char c : name)
        v.push_back((uint8_t)c);
}

void PushFloats(std::vector<uint8_t>& v, const std::vector<float>& values) {
    PushU16(v, kFloatList);
    PushU32(v, (uint32_t)values.size());
    for (const float f : values) {
        uint32_t bits = 0;
        static_assert(sizeof(bits) == sizeof(f));
        std::memcpy(&bits, &f, sizeof(bits));
        PushU32(v, bits);
    }
}

std::vector<uint8_t> BuildBinaryMesh() {
    std::vector<uint8_t> v;
    for (const char c : std::string_view("xof 0303bin 0032"))
        v.push_back((uint8_t)c);

    PushName(v, "Mesh");
    PushU16(v, kOBrace);
    PushU16(v, kInteger);
    PushU32(v, 3);
    PushFloats(v, {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 2.0F, 0.0F});
    PushU16(v, kInteger);
    PushU32(v, 1);
    PushU16(v, kIntegerList);
    PushU32(v, 4);
    PushU32(v, 3);
    PushU32(v, 0);
    PushU32(v, 1);
    PushU32(v, 2);
    PushU16(v, kCBrace);
    return v;
}

}

TEST_CASE("binary .x transcodes to the text form", "[xfile]") {
    const std::vector<uint8_t> binary = BuildBinaryMesh();
    const std::string source(binary.begin(), binary.end());
    std::string text;
    std::string err;
    REQUIRE(XFile::BinaryToText(source, text, err));
    REQUIRE(text.starts_with("xof 0303txt 0032"));
    REQUIRE(text.find("Mesh") != std::string::npos);
}

TEST_CASE("binary .x parses into the same scene as the text form", "[xfile]") {
    const std::vector<uint8_t> binary = BuildBinaryMesh();
    const std::string as_text(binary.begin(), binary.end());

    XFile::Scene scene;
    std::string err;
    REQUIRE(XFile::Parse(as_text, scene, err));
    REQUIRE(scene.frames.size() == 1);
    REQUIRE(scene.frames[0].meshes.size() == 1);

    const XFile::Mesh& mesh = scene.frames[0].meshes[0];
    REQUIRE(mesh.positions.size() == 3);
    REQUIRE(mesh.positions[2].y == 2.0F);
    REQUIRE(mesh.indices.size() == 3);
    REQUIRE(mesh.indices[2] == 2);
}

TEST_CASE("binary .x rejects an unknown token", "[xfile]") {
    std::vector<uint8_t> v;
    for (const char c : std::string_view("xof 0303bin 0032"))
        v.push_back((uint8_t)c);
    PushU16(v, 999);

    const std::string source(v.begin(), v.end());
    std::string text;
    std::string err;
    REQUIRE_FALSE(XFile::BinaryToText(source, text, err));
    REQUIRE(err.find("unknown binary .x token") != std::string::npos);
}
