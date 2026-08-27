#include <catch2/catch_test_macros.hpp>

#include "formats/sysidx.h"
#include "gc2d/gc_package.h"
#include "support/env.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace {

std::filesystem::path SysDir() {
    const std::optional<std::string> root = Support::EnvVar("R573_IIDX17_DIR");
    if (!root || root->empty()) return {};
    return std::filesystem::path(*root) / "data" / "graph_data" / "sys";
}

std::filesystem::path Sirius13Dir() {
    const std::optional<std::string> root = Support::EnvVar("R573_IIDX13_DIR");
    if (!root || root->empty()) return {};
    return std::filesystem::path(*root) / "data" / "graph" / "sys";
}

std::filesystem::path Ninth09Dir() {
    const std::optional<std::string> root = Support::EnvVar("R573_IIDX09_DIR");
    if (!root || root->empty()) return {};
    return std::filesystem::path(*root) / "data" / "graph";
}

std::filesystem::path Red11Dir() {
    const std::optional<std::string> root = Support::EnvVar("R573_IIDX11_DIR");
    if (!root || root->empty()) return {};
    return std::filesystem::path(*root) / "data" / "graph" / "sys";
}

int CheckCells(const SysIdx::Package& pkg, int& past_last_tile) {
    int straddling = 0;
    const int atlas_h = SysIdx::kAtlasTileHeight * (int)pkg.texture_paths.size();
    for (const auto& c : pkg.cells) {
        REQUIRE((int)c.x + (int)c.w <= SysIdx::kAtlasWidth);
        REQUIRE((int)c.y + (int)c.h <= SysIdx::kAtlasTileHeight * SysIdx::kMaxTextures);
        if ((int)c.y + (int)c.h > atlas_h) past_last_tile++;
        if ((c.y / SysIdx::kAtlasTileHeight) != ((c.y + c.h - 1) / SysIdx::kAtlasTileHeight)) {
            straddling++;
        }
    }
    return straddling;
}

void CheckRecords(const SysIdx::Package& pkg) {
    REQUIRE(pkg.records.back().type == SysIdx::kRecEndTable);
    for (const auto& r : pkg.records) {
        if (r.type == SysIdx::kRecDrawCell && r.id >= 0) {
            REQUIRE((size_t)r.id < pkg.cells.size());
        }
        if (r.type == SysIdx::kRecNested) {
            REQUIRE((size_t)r.id < pkg.records.size());
        }
    }
    for (const auto& [name, start] : pkg.animation_names) {
        REQUIRE((size_t)start < pkg.records.size());
        int longest = 0;
        for (size_t i = start; i < pkg.records.size(); i++) {
            if (pkg.records[i].type < 0) break;
            longest = (pkg.records[i].t_end > longest) ? pkg.records[i].t_end : longest;
        }
        REQUIRE(SysIdx::AnimationLength(pkg, start) == longest);
    }
}

}

TEST_CASE("sysidx parses a synthetic index end to end") {
    std::vector<uint8_t> c0(0x1B8 + 24, 0);
    auto put16 = [](std::vector<uint8_t>& v, size_t o, uint16_t x) {
        v[o] = (uint8_t)(x & 0xFFU);
        v[o + 1] = (uint8_t)(x >> 8);
    };
    auto put32 = [](std::vector<uint8_t>& v, size_t o, uint32_t x) {
        for (int i = 0; i < 4; i++)
            v[o + (size_t)i] = (uint8_t)((x >> (8 * i)) & 0xFFU);
    };
    put16(c0, 0x002, 1);
    put32(c0, 0x004, 0x1B8);
    put32(c0, 0x010, 0);
    const std::string path = "/sys/test/0.gcz";
    for (size_t i = 0; i < path.size(); i++)
        c0[0x014 + i] = (uint8_t)path[i];
    put16(c0, 0x1B8 + 0, 10);
    put16(c0, 0x1B8 + 2, 20);
    put16(c0, 0x1B8 + 4, 64);
    put16(c0, 0x1B8 + 6, 32);

    std::vector<uint8_t> c1;
    auto push_name = [&c1](const std::string& n, uint16_t v) {
        for (char ch : n)
            c1.push_back((uint8_t)ch);
        c1.push_back(0);
        c1.push_back((uint8_t)(v & 0xFFU));
        c1.push_back((uint8_t)(v >> 8));
    };
    push_name("logo", 0);
    c1.push_back(0);
    c1.push_back(0);
    push_name("intro", 0);
    c1.push_back(0);

    std::vector<uint8_t> file;
    auto append32 = [&file](uint32_t x) {
        for (int i = 0; i < 4; i++)
            file.push_back((uint8_t)((x >> (8 * i)) & 0xFFU));
    };
    append32((uint32_t)c0.size());
    file.insert(file.end(), c0.begin(), c0.end());
    append32((uint32_t)c1.size());
    file.insert(file.end(), c1.begin(), c1.end());

    SysIdx::Package pkg;
    std::string err;
    REQUIRE(SysIdx::Parse(file, pkg, err));
    REQUIRE(pkg.texture_count == 1);
    REQUIRE(pkg.texture_paths.size() == 1);
    REQUIRE(pkg.texture_paths[0] == "/sys/test/0.gcz");
    REQUIRE(pkg.cells.size() == 1);
    REQUIRE(pkg.cells[0].x == 10);
    REQUIRE(pkg.cells[0].w == 64);
    REQUIRE(pkg.cell_names.at("logo") == 0);
    REQUIRE(pkg.animation_names.at("intro") == 0);
}

TEST_CASE("an animation's declared length can outrun the records that draw it") {
    SysIdx::Package pkg;
    SysIdx::Record draw;
    draw.type = SysIdx::kRecDrawCell;
    draw.id = 0;
    draw.t_start = 0;
    draw.t_end = 30;
    SysIdx::Record end;
    end.type = SysIdx::kRecEndAnimation;
    end.t_end = 90;
    pkg.records.push_back(draw);
    pkg.records.push_back(end);

    CHECK(SysIdx::AnimationLength(pkg, 0) == 90);
    CHECK(SysIdx::AnimationContentEnd(pkg, 0) == 30);
}

TEST_CASE("sysidx rejects a chunk chain that does not tile the file") {
    std::vector<uint8_t> file(16, 0);
    file[0] = 0xFF;
    SysIdx::Package pkg;
    std::string err;
    REQUIRE_FALSE(SysIdx::Parse(file, pkg, err));
}

TEST_CASE("sirius index invariants hold on the real IIDX 17 data", "[real]") {
    const std::filesystem::path sys = SysDir();
    if (sys.empty() || !std::filesystem::exists(sys)) {
        SKIP("R573_IIDX17_DIR not set or data missing");
    }

    int packages = 0;
    int encrypted = 0;
    int with_records = 0;
    int cells_total = 0;
    int straddling = 0;
    int past_last_tile = 0;
    int tiles_total = 0;

    for (const auto& e : std::filesystem::directory_iterator(sys)) {
        if (!e.is_directory()) continue;
        const std::string dir = e.path().string();
        if (!Gc2d::IsPackageDir(dir)) continue;
        if (!std::filesystem::exists(e.path() / "system.idx")) encrypted++;

        Gc2d::Package loaded;
        std::string err;
        REQUIRE(Gc2d::Load(dir, loaded, err));
        const SysIdx::Package& pkg = loaded.index;
        packages++;

        REQUIRE(pkg.texture_count == pkg.texture_paths.size());
        REQUIRE(pkg.texture_paths.size() <= (size_t)SysIdx::kMaxTextures);
        REQUIRE(pkg.cell_names.size() == pkg.cells.size());
        REQUIRE(loaded.tiles.size() == pkg.texture_paths.size());
        cells_total += (int)pkg.cells.size();

        straddling += CheckCells(pkg, past_last_tile);

        if (!pkg.records.empty()) {
            with_records++;
            CheckRecords(pkg);
        }

        for (const auto& tile : loaded.tiles) {
            REQUIRE(tile.width > 0);
            REQUIRE(tile.width <= SysIdx::kAtlasWidth);
            REQUIRE(tile.height <= SysIdx::kAtlasTileHeight);
            REQUIRE(tile.bgra.size() == (size_t)tile.width * (size_t)tile.height * 4);
            tiles_total++;
        }
    }

    REQUIRE(packages > 340);
    REQUIRE(encrypted > 130);
    REQUIRE(with_records > 200);
    REQUIRE(cells_total > 20000);
    REQUIRE(straddling > 0);
    REQUIRE(past_last_tile == 0);
    REQUIRE(tiles_total > 1300);
}

TEST_CASE("distorted packages load through the blowfish texture path", "[real]") {
    const std::filesystem::path sys = Sirius13Dir();
    if (sys.empty() || !std::filesystem::exists(sys)) {
        SKIP("R573_IIDX13_DIR not set or data missing");
    }

    int packages = 0;
    int tiles_total = 0;
    int cells_total = 0;
    int past_last_tile = 0;

    for (const auto& e : std::filesystem::directory_iterator(sys)) {
        if (!e.is_directory()) continue;
        const std::string dir = e.path().string();
        if (!Gc2d::IsPackageDir(dir)) continue;

        Gc2d::Package loaded;
        std::string err;
        REQUIRE(Gc2d::Load(dir, loaded, err));
        packages++;
        cells_total += (int)loaded.index.cells.size();
        REQUIRE(loaded.tiles.size() == loaded.index.texture_paths.size());
        for (const auto& tile : loaded.tiles) {
            REQUIRE(tile.width > 0);
            REQUIRE(tile.width <= SysIdx::kAtlasWidth);
            REQUIRE(tile.height <= SysIdx::kAtlasTileHeight);
            REQUIRE(tile.bgra.size() == (size_t)tile.width * (size_t)tile.height * 4);
            tiles_total++;
        }
        CheckCells(loaded.index, past_last_tile);
        if (!loaded.index.records.empty()) CheckRecords(loaded.index);
    }

    REQUIRE(packages > 200);
    REQUIRE(tiles_total > 700);
    REQUIRE(cells_total > 5000);
    REQUIRE(past_last_tile == 26);
}

TEST_CASE("iidx red packages load with no encryption at all", "[real]") {
    const std::filesystem::path sys = Red11Dir();
    if (sys.empty() || !std::filesystem::exists(sys)) {
        SKIP("R573_IIDX11_DIR not set or data missing");
    }

    int packages = 0;
    int tiles_total = 0;
    int cells_total = 0;
    int past_last_tile = 0;

    for (const auto& e : std::filesystem::directory_iterator(sys)) {
        if (!e.is_directory()) continue;
        const std::string dir = e.path().string();
        if (!Gc2d::IsPackageDir(dir)) continue;

        Gc2d::Package loaded;
        std::string err;
        REQUIRE(Gc2d::Load(dir, loaded, err));
        packages++;
        cells_total += (int)loaded.index.cells.size();
        REQUIRE(loaded.tiles.size() == loaded.index.texture_paths.size());
        for (const auto& tile : loaded.tiles) {
            REQUIRE(tile.width > 0);
            REQUIRE(tile.bgra.size() == (size_t)tile.width * (size_t)tile.height * 4);
            tiles_total++;
        }
        CheckCells(loaded.index, past_last_tile);
        if (!loaded.index.records.empty()) CheckRecords(loaded.index);
    }

    REQUIRE(packages > 150);
    REQUIRE(tiles_total > 300);
    REQUIRE(cells_total > 3000);
    REQUIRE(past_last_tile == 0);
}

TEST_CASE("9th style packages load from the single big-endian chunk", "[real]") {
    const std::filesystem::path graph = Ninth09Dir();
    if (graph.empty() || !std::filesystem::exists(graph)) {
        SKIP("R573_IIDX09_DIR not set or data missing");
    }

    int packages = 0;
    int tiles_total = 0;
    int cells_total = 0;
    int named_cells = 0;
    int with_animations = 0;
    int past_last_tile = 0;

    for (const auto& e : std::filesystem::recursive_directory_iterator(graph)) {
        if (!e.is_directory()) continue;
        const std::string dir = e.path().string();
        if (!Gc2d::IsPackageDir(dir)) continue;

        Gc2d::Package loaded;
        std::string err;
        REQUIRE(Gc2d::Load(dir, loaded, err));
        packages++;
        cells_total += (int)loaded.index.cells.size();
        named_cells += (int)loaded.index.cell_names.size();
        if (!loaded.index.animation_names.empty()) with_animations++;
        REQUIRE(loaded.tiles.size() == loaded.index.texture_paths.size());
        for (const auto& tile : loaded.tiles) {
            REQUIRE(tile.width > 0);
            REQUIRE(tile.bgra.size() == (size_t)tile.width * (size_t)tile.height * 4);
            tiles_total++;
        }
        CheckCells(loaded.index, past_last_tile);
        if (!loaded.index.records.empty()) CheckRecords(loaded.index);

        for (const auto& [name, start] : loaded.index.animation_names)
            REQUIRE((size_t)start < loaded.index.records.size());
    }

    REQUIRE(packages == 170);
    REQUIRE(tiles_total > 300);
    REQUIRE(cells_total > 5000);
    REQUIRE(named_cells == cells_total);
    REQUIRE(with_animations > 80);
    REQUIRE(past_last_tile == 0);
}
