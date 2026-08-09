#include "gui_mock_assets.h"

#include <cstddef>
#include <filesystem>
#include <cstdint>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <vector>

namespace GuiTest {

namespace {

constexpr size_t kSysIdxHeaderPaths = 0x014;
constexpr size_t kSysIdxPathSlotBytes = 32;
constexpr size_t kSysIdxFixedHeaderBytes = 0x1B8;
constexpr size_t kSysIdxRecordBytes = 36;
constexpr size_t kGczHeaderBytes = 24;

void PushU16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(static_cast<uint8_t>(x & 0xFFU));
    v.push_back(static_cast<uint8_t>(x >> 8U));
}

void PushU32(std::vector<uint8_t>& v, uint32_t x) {
    for (int i = 0; i < 4; i++)
        v.push_back(static_cast<uint8_t>((x >> (8U * static_cast<unsigned>(i))) & 0xFFU));
}

void PushBe16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(static_cast<uint8_t>(x >> 8U));
    v.push_back(static_cast<uint8_t>(x & 0xFFU));
}

void PushBe32(std::vector<uint8_t>& v, uint32_t x) {
    for (int i = 3; i >= 0; i--)
        v.push_back(static_cast<uint8_t>((x >> (8U * static_cast<unsigned>(i))) & 0xFFU));
}

void PushCString(std::vector<uint8_t>& v, const std::string& s) {
    v.insert(v.end(), s.begin(), s.end());
    v.push_back(0);
}

void PutU16At(std::vector<uint8_t>& v, size_t at, uint16_t x) {
    v[at] = static_cast<uint8_t>(x & 0xFFU);
    v[at + 1] = static_cast<uint8_t>(x >> 8U);
}

void PutU32At(std::vector<uint8_t>& v, size_t at, uint32_t x) {
    for (int i = 0; i < 4; i++) {
        v[at + static_cast<size_t>(i)] =
            static_cast<uint8_t>((x >> (8U * static_cast<unsigned>(i))) & 0xFFU);
    }
}

std::vector<uint8_t> LzssLiteralStream(const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> out;
    PushU32(out, static_cast<uint32_t>(payload.size()));
    for (size_t i = 0; i < payload.size(); i++) {
        if (i % 8 == 0) out.push_back(0xFF);
        out.push_back(payload[i]);
    }
    return out;
}

std::vector<uint8_t> LzssLiteralText(const std::string& text) {
    return LzssLiteralStream(std::vector<uint8_t>(text.begin(), text.end()));
}

std::vector<uint8_t> GczTile(int width, int height) {
    std::vector<uint8_t> v = {'G', 'C', ' ', 0};
    const auto pixel_bytes =
        static_cast<uint32_t>(static_cast<size_t>(width) * static_cast<size_t>(height) * 2);
    PushBe32(v, static_cast<uint32_t>(kGczHeaderBytes) + pixel_bytes);
    PushBe32(v, 0);
    PushBe16(v, static_cast<uint16_t>(width));
    PushBe16(v, static_cast<uint16_t>(height));
    PushBe32(v, 0);
    PushBe32(v, pixel_bytes);
    const uint16_t opaque_white = 0xFFFFU;
    for (uint32_t i = 0; i < pixel_bytes / 2; i++)
        PushU16(v, opaque_white);
    return v;
}

void WriteBytes(const std::filesystem::path& file, const std::vector<uint8_t>& bytes) {
    std::ofstream f(file, std::ios::binary | std::ios::trunc);
    for (uint8_t const b : bytes)
        f.put(static_cast<char>(b));
}

void PushSysIdxRecord(std::vector<uint8_t>& c0, int16_t type, int16_t id, int16_t t_end) {
    const size_t start = c0.size();
    c0.resize(start + kSysIdxRecordBytes, 0);
    PutU16At(c0, start, static_cast<uint16_t>(type));
    PutU16At(c0, start + 2, static_cast<uint16_t>(id));
    PutU16At(c0, start + 10, static_cast<uint16_t>(t_end));
}

std::vector<uint8_t> BuildSysIdxChunk0() {
    std::vector<uint8_t> c0(kSysIdxFixedHeaderBytes, 0);
    PutU16At(c0, 0x002, 1);

    const std::string texture = "tex0.gcz";
    for (size_t i = 0; i < texture.size(); i++)
        c0[kSysIdxHeaderPaths + i] = static_cast<uint8_t>(texture[i]);
    static_assert(kSysIdxHeaderPaths + kSysIdxPathSlotBytes < kSysIdxFixedHeaderBytes);

    PutU32At(c0, 0x004, static_cast<uint32_t>(kSysIdxFixedHeaderBytes));
    for (uint16_t const cell_x : {uint16_t{0}, uint16_t{16}}) {
        PushU16(c0, cell_x);
        PushU16(c0, 0);
        PushU16(c0, 16);
        PushU16(c0, 16);
    }
    PushU32(c0, 0);
    PushU32(c0, 0);

    PutU32At(c0, 0x010, static_cast<uint32_t>(c0.size()));
    PushSysIdxRecord(c0, 0, 0, 0);
    PushSysIdxRecord(c0, -1, 0, 30);
    PushSysIdxRecord(c0, 0, 1, 0);
    PushSysIdxRecord(c0, -1, 0, 45);
    PushSysIdxRecord(c0, -2, 0, 0);
    return c0;
}

std::vector<uint8_t> BuildSysIdxChunk1() {
    std::vector<uint8_t> c1;
    PushCString(c1, "cell_left");
    PushU16(c1, 0);
    PushCString(c1, "cell_right");
    PushU16(c1, 1);
    c1.push_back(0);

    c1.push_back(0);

    PushCString(c1, "anim_intro");
    PushU16(c1, 0);
    PushCString(c1, "anim_loop");
    PushU16(c1, 2);
    c1.push_back(0);
    return c1;
}

std::string MockSceneManifest() {
    return "[image_file]\n"
           "out/model/mock/0,0\n"
           "\n"
           "[pattern_list]\n"
           "/u/home/in/model/mock/mock_tex.bmp = 0,0,64,64\n";
}

std::string MockModelText(bool with_camera) {
    std::string text = "xof 0303txt 0032\n"
                       "Frame Frame_SCENE_ROOT {\n"
                       "  FrameTransformMatrix { 1.0,0,0,0, 0,1.0,0,0, 0,0,1.0,0, 0,0,0,1.0;; }\n"
                       "  Frame Frame1_quad {\n"
                       "    FrameTransformMatrix { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1;; }\n"
                       "    Mesh {\n"
                       "      3;\n"
                       "      0.0;0.0;0.0;,\n"
                       "      10.0;0.0;0.0;,\n"
                       "      0.0;10.0;0.0;;\n"
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
                       "          TextureFilename { \"mock_tex.bmp\"; }\n"
                       "        }\n"
                       "      }\n"
                       "      MeshTextureCoords {\n"
                       "        3;\n"
                       "        0.0;0.0;,\n"
                       "        1.0;0.0;,\n"
                       "        0.0;1.0;;\n"
                       "      }\n"
                       "    }\n"
                       "  }\n";
    if (with_camera) {
        text += "  Frame Frame2_camera {\n"
                "    FrameTransformMatrix { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-40.0,1;; }\n"
                "  }\n";
    }
    text += "}\n"
            "AnimationSet Set0 {\n"
            "  Animation Anim0 {\n"
            "    { Frame1_quad }\n"
            "    AnimationKey {\n"
            "      2;\n"
            "      2;\n"
            "      0;3;0.0,0.0,0.0;;,\n"
            "      600;3;5.0,0.0,0.0;;;\n"
            "    }\n"
            "  }\n"
            "}\n";
    return text;
}

}

TempAssetDir::TempAssetDir(const char* name) {
    std::error_code ec;
    dir_ = std::filesystem::temp_directory_path(ec) / (std::string("r573_gui_") + name);
    std::filesystem::remove_all(dir_, ec);
    std::filesystem::create_directories(dir_, ec);
    path_ = dir_.string();
}

TempAssetDir::~TempAssetDir() {
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
}

void WriteMock2dPackage(const std::string& dir) {
    const std::filesystem::path path(dir);
    const std::vector<uint8_t> c0 = BuildSysIdxChunk0();
    const std::vector<uint8_t> c1 = BuildSysIdxChunk1();

    std::vector<uint8_t> file;
    PushU32(file, static_cast<uint32_t>(c0.size()));
    file.insert(file.end(), c0.begin(), c0.end());
    PushU32(file, static_cast<uint32_t>(c1.size()));
    file.insert(file.end(), c1.begin(), c1.end());

    WriteBytes(path / "system.idx", file);
    WriteBytes(path / "tex0.gcz", LzssLiteralStream(GczTile(4, 4)));
}

void WriteMock3dScene(const std::string& dir, bool with_camera) {
    const std::filesystem::path path(dir);
    WriteBytes(path / "mock.inz", LzssLiteralText(MockSceneManifest()));
    WriteBytes(path / "0.gcz", LzssLiteralStream(GczTile(8, 8)));
    WriteBytes(path / "mock_model.xz", LzssLiteralText(MockModelText(with_camera)));
}

}
