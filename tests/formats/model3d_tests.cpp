#include <catch2/catch_test_macros.hpp>

#include "formats/gcz.h"
#include "formats/inz.h"
#include "formats/lzss.h"
#include "formats/xfile.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> LzssLiteralStream(const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> out;
    const auto n = (uint32_t)payload.size();
    out.push_back((uint8_t)(n & 0xFFU));
    out.push_back((uint8_t)((n >> 8) & 0xFFU));
    out.push_back((uint8_t)((n >> 16) & 0xFFU));
    out.push_back((uint8_t)((n >> 24) & 0xFFU));
    for (size_t i = 0; i < payload.size(); i++) {
        if (i % 8 == 0) out.push_back(0xFF);
        out.push_back(payload[i]);
    }
    return out;
}

void PushBe16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back((uint8_t)(x >> 8));
    v.push_back((uint8_t)(x & 0xFFU));
}

void PushBe32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x >> 24));
    v.push_back((uint8_t)((x >> 16) & 0xFFU));
    v.push_back((uint8_t)((x >> 8) & 0xFFU));
    v.push_back((uint8_t)(x & 0xFFU));
}

}

TEST_CASE("lzss round-trips an all-literal stream") {
    const std::vector<uint8_t> payload = {'H', 'e', 'l', 'l', 'o', ' ', '5', '7', '3', '!'};
    std::vector<uint8_t> out;
    std::string err;
    REQUIRE(Lzss::Decompress(LzssLiteralStream(payload), out, err));
    REQUIRE(out == payload);
}

TEST_CASE("lzss expands a back-reference through the ring") {
    std::vector<uint8_t> s;
    PushBe32(s, 0);
    s[0] = 8;
    s[1] = 0;
    s[2] = 0;
    s[3] = 0;
    s.push_back(0x0F);
    for (char c : std::string("ABCD"))
        s.push_back((uint8_t)c);
    s.push_back(4078 & 0xFF);
    s.push_back((uint8_t)(((4078 >> 4) & 0xF0) | 1));

    std::vector<uint8_t> out;
    std::string err;
    REQUIRE(Lzss::Decompress(s, out, err));
    REQUIRE(out.size() == 8);
    REQUIRE(std::string(out.begin(), out.begin() + 4) == "ABCD");
    REQUIRE(std::string(out.begin() + 4, out.end()) == "ABCD");
}

TEST_CASE("lzss reports a truncated stream instead of returning short data") {
    std::vector<uint8_t> s = {0x40, 0x00, 0x00, 0x00, 0xFF, 'a'};
    std::vector<uint8_t> out;
    std::string err;
    REQUIRE_FALSE(Lzss::Decompress(s, out, err));
    REQUIRE(err.find("ended after") != std::string::npos);
}

TEST_CASE("gcz parses the big-endian GC header and expands A1R5G5B5") {
    std::vector<uint8_t> v = {'G', 'C', ' ', 0};
    PushBe32(v, 24 + 8);
    PushBe32(v, 0);
    PushBe16(v, 2);
    PushBe16(v, 2);
    PushBe32(v, 0);
    PushBe32(v, 8);
    const uint16_t opaque_red = 0x8000U | (31U << 10U);
    const uint16_t transparent = 0x0000U;
    for (const uint16_t px : {opaque_red, transparent, opaque_red, transparent}) {
        v.push_back((uint8_t)(px & 0xFFU));
        v.push_back((uint8_t)(px >> 8));
    }

    Gcz::Tile tile;
    std::string err;
    REQUIRE(Gcz::Parse(v, tile, err));
    REQUIRE(tile.width == 2);
    REQUIRE(tile.height == 2);

    std::vector<uint8_t> bgra;
    Gcz::ExpandToBgra(tile, bgra);
    REQUIRE(bgra.size() == 16);
    REQUIRE(bgra[2] == 255);
    REQUIRE(bgra[3] == 255);
    REQUIRE(bgra[7] == 0);
}

TEST_CASE("gcz rejects a payload size that disagrees with the dimensions") {
    std::vector<uint8_t> v = {'G', 'C', ' ', 0};
    PushBe32(v, 32);
    PushBe32(v, 0);
    PushBe16(v, 4);
    PushBe16(v, 4);
    PushBe32(v, 0);
    PushBe32(v, 8);
    Gcz::Tile tile;
    std::string err;
    REQUIRE_FALSE(Gcz::Parse(v, tile, err));
    REQUIRE(err.find("disagrees") != std::string::npos);
}

TEST_CASE("inz parses slices and pattern rects and resolves by basename") {
    const std::string text = "[image_file]\n"
                             "out/model/mode_bg/0,0\n"
                             "out/model/mode_bg/1,16\n"
                             "\n"
                             "[pattern_list]\n"
                             "/u/home/in/model/mode_bg/bill.bmp = 0,0,256,256\n"
                             "/u/home/in/model/mode_bg/cloud1.bmp = 0,256,128,64\n";

    Inz::Manifest m;
    std::string err;
    REQUIRE(Inz::Parse(text, m, err));
    REQUIRE(m.slices.size() == 2);
    REQUIRE(m.slices[0].name == "0");
    REQUIRE(m.slices[0].flag == 0);
    REQUIRE(m.slices[1].name == "1");
    REQUIRE(m.slices[1].flag == 16);

    REQUIRE(m.patterns.size() == 2);
    const Inz::Pattern* p = Inz::FindPattern(m, "cloud1.bmp");
    REQUIRE(p != nullptr);
    REQUIRE(p->x == 0);
    REQUIRE(p->y == 256);
    REQUIRE(p->w == 128);
    REQUIRE(p->h == 64);
    REQUIRE(Inz::FindPattern(m, "nope.bmp") == nullptr);
}

TEST_CASE("xfile parses a frame hierarchy with a textured mesh") {
    const std::string text =
        "xof 0303txt 0032\n"
        "template Meaningless { <0000> DWORD a; }\n"
        "Frame Frame_SCENE_ROOT {\n"
        "  FrameTransformMatrix { 1.0,0,0,0, 0,1.0,0,0, 0,0,1.0,0, 0,0,0,1.0;; }\n"
        "  Frame Frame1_layer {\n"
        "    FrameTransformMatrix { 1,0,0,0, 0,1,0,0, 0,0,1,0, 5.0,6.0,7.0,1;; }\n"
        "    Mesh {\n"
        "      3;\n"
        "      0.0;0.0;0.0;,\n"
        "      1.0;0.0;0.0;,\n"
        "      0.0;1.0;0.0;;\n"
        "      1;\n"
        "      3;0,1,2;;\n"
        "      MeshMaterialList {\n"
        "        1;\n"
        "        1;\n"
        "        0;;\n"
        "        Material {\n"
        "          1.0;1.0;1.0;1.0;;\n"
        "          0.0;\n"
        "          0.0;0.0;0.0;;\n"
        "          0.0;0.0;0.0;;\n"
        "          TextureFilename { \"island_tex01.bmp\"; }\n"
        "        }\n"
        "      }\n"
        "      MeshTextureCoords {\n"
        "        3;\n"
        "        0.0;0.0;,\n"
        "        1.0;0.0;,\n"
        "        0.0;1.0;;\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "}\n";

    XFile::Scene scene;
    std::string err;
    REQUIRE(XFile::Parse(text, scene, err));
    REQUIRE(scene.frames.size() == 2);
    REQUIRE(scene.frames[0].name == "Frame_SCENE_ROOT");
    REQUIRE(scene.frames[1].name == "Frame1_layer");
    REQUIRE(scene.frames[1].parent == 0);
    REQUIRE(scene.frames[0].children.size() == 1);

    REQUIRE(scene.frames[1].transform[12] == 5.0F);
    REQUIRE(scene.frames[1].transform[13] == 6.0F);

    REQUIRE(scene.frames[1].meshes.size() == 1);
    const auto& mesh = scene.frames[1].meshes[0];
    REQUIRE(mesh.positions.size() == 3);
    REQUIRE(mesh.positions[1].x == 1.0F);
    REQUIRE(mesh.indices.size() == 3);
    REQUIRE(mesh.uvs.size() == 3);
    REQUIRE(mesh.materials.size() == 1);
    REQUIRE(mesh.materials[0].texture == "island_tex01.bmp");
}

TEST_CASE("xfile triangulates a quad face into two triangles") {
    const std::string text = "xof 0303txt 0032\n"
                             "Frame f {\n"
                             "  Mesh {\n"
                             "    4;\n"
                             "    0;0;0;,1;0;0;,1;1;0;,0;1;0;;\n"
                             "    1;\n"
                             "    4;0,1,2,3;;\n"
                             "  }\n"
                             "}\n";
    XFile::Scene scene;
    std::string err;
    REQUIRE(XFile::Parse(text, scene, err));
    REQUIRE(scene.frames[0].meshes[0].indices.size() == 6);
    REQUIRE(scene.frames[0].meshes[0].face_material.size() == 2);
}

TEST_CASE("xfile collects animation keys per frame channel") {
    const std::string text = "xof 0303txt 0032\n"
                             "Frame target { }\n"
                             "AnimationSet Set0 {\n"
                             "  Animation Anim0 {\n"
                             "    { target }\n"
                             "    AnimationKey {\n"
                             "      0;\n"
                             "      2;\n"
                             "      0;4;1.0,0.0,0.0,0.0;;,\n"
                             "      30;4;0.0,1.0,0.0,0.0;;;\n"
                             "    }\n"
                             "    AnimationKey {\n"
                             "      2;\n"
                             "      1;\n"
                             "      0;3;1.0,2.0,3.0;;;\n"
                             "    }\n"
                             "  }\n"
                             "}\n";
    XFile::Scene scene;
    std::string err;
    REQUIRE(XFile::Parse(text, scene, err));
    REQUIRE(scene.channels.size() == 1);
    REQUIRE(scene.channels[0].frame_name == "target");
    REQUIRE(scene.channels[0].rotation.size() == 2);
    REQUIRE(scene.channels[0].rotation[1].time == 30);
    REQUIRE(scene.channels[0].position.size() == 1);
    REQUIRE(scene.channels[0].position[0].value[1] == 2.0F);
    REQUIRE(scene.max_key_time == 30);
}

TEST_CASE("xfile rejects the binary form rather than misparsing it") {
    XFile::Scene scene;
    std::string err;
    REQUIRE_FALSE(XFile::Parse("xof 0303bin 0032\n", scene, err));
    REQUIRE(err.find("text .x") != std::string::npos);
}
