#include <catch2/catch_test_macros.hpp>

#include "formats/gcz.h"
#include "formats/inz.h"
#include "formats/lzss.h"
#include "formats/xfile.h"
#include "support/env.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::filesystem::path SceneDir() {
    const std::optional<std::string> root = Support::EnvVar("R573_IIDX18_DIR");
    if (!root || root->empty()) return {};
    return std::filesystem::path(*root) / "data" / "graph_data" / "model" / "mode_bg";
}

std::vector<uint8_t> ReadAll(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

std::vector<uint8_t> Inflate(const std::filesystem::path& p) {
    std::vector<uint8_t> out;
    std::string err;
    const std::vector<uint8_t> raw = ReadAll(p);
    REQUIRE_FALSE(raw.empty());
    REQUIRE(Lzss::Decompress(raw, out, err));
    return out;
}

}

TEST_CASE("mode_bg scene decodes from the real IIDX 18 data", "[.real]") {
    const std::filesystem::path dir = SceneDir();
    if (dir.empty() || !std::filesystem::exists(dir)) return;

    const std::vector<uint8_t> manifest_bytes = Inflate(dir / "mode_bg.inz");
    Inz::Manifest manifest;
    std::string err;
    REQUIRE(Inz::Parse(std::string(manifest_bytes.begin(), manifest_bytes.end()), manifest, err));
    REQUIRE(manifest.slices.size() == 14);
    REQUIRE_FALSE(manifest.patterns.empty());

    int total_height = 0;
    for (const auto& slice : manifest.slices) {
        const std::vector<uint8_t> tile_bytes = Inflate(dir / (slice.name + ".gcz"));
        Gcz::Tile tile;
        REQUIRE(Gcz::Parse(tile_bytes, tile, err));
        REQUIRE(tile.width == 256);
        REQUIRE(tile.height == 256);
        REQUIRE(tile.pixels.size() == (size_t)256 * 256 * 2);
        total_height += tile.height;
    }
    REQUIRE(total_height == 14 * 256);

    for (const auto& p : manifest.patterns) {
        REQUIRE(p.x >= 0);
        REQUIRE(p.y + p.h <= total_height);
        REQUIRE(p.x + p.w <= 256);
    }

    for (const char* model : {"island.xz", "sea.xz", "cloud.xz"}) {
        const std::vector<uint8_t> text_bytes = Inflate(dir / model);
        XFile::Scene scene;
        REQUIRE(XFile::Parse(std::string(text_bytes.begin(), text_bytes.end()), scene, err));
        REQUIRE_FALSE(scene.frames.empty());
        REQUIRE(scene.frames[0].name == "Frame_SCENE_ROOT");

        size_t tris = 0;
        size_t textured = 0;
        for (const auto& f : scene.frames) {
            for (const auto& m : f.meshes) {
                REQUIRE(m.indices.size() % 3 == 0);
                tris += m.indices.size() / 3;
                for (const uint32_t idx : m.indices)
                    REQUIRE(idx < m.positions.size());
                for (const auto& mat : m.materials) {
                    if (mat.texture.empty()) continue;
                    textured++;
                    REQUIRE(Inz::FindPattern(manifest, mat.texture) != nullptr);
                }
            }
        }
        REQUIRE(tris > 0);
        REQUIRE(textured > 0);
        REQUIRE_FALSE(scene.channels.empty());
        REQUIRE(scene.max_key_time > 0);
    }
}
