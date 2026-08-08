#include "gc2d/gc_package.h"

#include "formats/aes.h"
#include "formats/gcz.h"
#include "formats/lzss.h"
#include "formats/sysidx.h"
#include "support/log.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace Gc2d {

namespace {

constexpr std::array<uint8_t, Aes::kKeyBytes> kKeyTableA = {
    0x40, 0xbc, 0x0b, 0x71, 0xfb, 0x2e, 0xd7, 0x20, 0x3e, 0x19, 0x93, 0x31, 0x62, 0xc8, 0xe4, 0xb5,
    0xe8, 0x7c, 0x8d, 0xb1, 0xb8, 0x00, 0x07, 0x64, 0x34, 0x39, 0x30, 0xe1, 0x56, 0xff, 0x6a, 0x97};

constexpr std::array<uint8_t, Aes::kKeyBytes> kKeyTableB = {
    0x51, 0x8b, 0xd9, 0x02, 0x98, 0xcb, 0x98, 0x56, 0xba, 0x95, 0xba, 0xc0, 0x74, 0xfa, 0xb0, 0xc0,
    0x57, 0xa4, 0xf3, 0x48, 0x9b, 0xa1, 0xda, 0x1d, 0xb8, 0x1f, 0xc7, 0xd3, 0xa1, 0x19, 0x89, 0xf2};

std::vector<uint8_t> ReadFile(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

std::string LeafName(const std::string& vfs_path) {
    const size_t slash = vfs_path.find_last_of("/\\");
    return (slash == std::string::npos) ? vfs_path : vfs_path.substr(slash + 1);
}

std::array<uint8_t, Aes::kKeyBytes> DeriveKey(const std::string& package_name) {
    std::array<uint8_t, Aes::kKeyBytes> key{};
    for (size_t i = 0; i < Aes::kKeyBytes; i++) {
        const uint8_t named = (i < package_name.size()) ? (uint8_t)package_name[i] : (uint8_t)0;
        key[i] = (uint8_t)(kKeyTableA[i] ^ kKeyTableB[i] ^ named);
    }
    return key;
}

bool ReadMaybeEncrypted(const std::filesystem::path& plain, const std::filesystem::path& sealed,
                        const std::string& package_name, std::vector<uint8_t>& out,
                        std::string& err) {
    std::error_code ec;
    if (std::filesystem::exists(plain, ec)) {
        out = ReadFile(plain);
        if (out.empty()) {
            err = "could not read " + plain.filename().string();
            return false;
        }
        return true;
    }
    const std::vector<uint8_t> raw = ReadFile(sealed);
    if (raw.size() <= Aes::kBlockBytes) {
        err = "could not read " + sealed.filename().string();
        return false;
    }
    const std::array<uint8_t, Aes::kKeyBytes> key = DeriveKey(package_name);
    return Aes::DecryptCbcCts(key, std::span(raw).first(Aes::kBlockBytes),
                              std::span(raw).subspan(Aes::kBlockBytes), out, err);
}

std::filesystem::path SealedName(const std::filesystem::path& plain) {
    std::filesystem::path sealed = plain;
    sealed.replace_extension(plain.extension() == ".gcz" ? ".gcr" : ".idr");
    return sealed;
}

bool LoadTextures(const std::filesystem::path& dir, Package& out, std::string& err) {
    out.tiles.clear();
    out.tiles.reserve(out.index.texture_paths.size());
    for (const auto& vfs : out.index.texture_paths) {
        const std::filesystem::path file = dir / LeafName(vfs);
        std::vector<uint8_t> raw;
        if (!ReadMaybeEncrypted(file, SealedName(file), out.name, raw, err)) return false;
        std::vector<uint8_t> payload;
        if (!Lzss::Decompress(raw, payload, err)) return false;
        Gcz::Tile parsed;
        if (!Gcz::Parse(payload, parsed, err)) return false;
        TileImage tile;
        tile.width = parsed.width;
        tile.height = parsed.height;
        tile.origin_x = parsed.origin_x;
        tile.origin_y = parsed.origin_y;
        Gcz::ExpandToBgra(parsed, tile.bgra);
        out.tiles.push_back(std::move(tile));
    }
    return true;
}

}

bool IsPackageDir(const std::string& dir) {
    std::error_code ec;
    const std::filesystem::path p(dir);
    if (!std::filesystem::is_directory(p, ec)) return false;
    return std::filesystem::exists(p / "system.idx", ec) ||
           std::filesystem::exists(p / "system.idr", ec);
}

bool Load(const std::string& dir, Package& out, std::string& err) {
    const std::filesystem::path path(dir);
    out = Package{};
    out.name = path.filename().string();

    std::vector<uint8_t> raw;
    if (!ReadMaybeEncrypted(path / "system.idx", path / "system.idr", out.name, raw, err))
        return false;
    if (!SysIdx::Parse(raw, out.index, err)) return false;
    if (!LoadTextures(path, out, err)) return false;

    out.animation_names.reserve(out.index.animation_names.size());
    for (const auto& [name, start] : out.index.animation_names)
        out.animation_names.push_back(name);
    std::ranges::sort(out.animation_names);

    LOG("Gc2d", "package '%s': %zu cells, %zu records, %zu animations, %zu tiles", out.name.c_str(),
        out.index.cells.size(), out.index.records.size(), out.animation_names.size(),
        out.tiles.size());
    return true;
}

}
