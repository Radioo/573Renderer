#include "game_fingerprint.h"

#include "support/log.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

namespace GameFingerprint {

namespace {

constexpr int kSearchDepth = 3;

const std::vector<Build> kBuilds = {
    Build{
        .id = "iidx10",
        .name = "IIDX 10th style (D01 JAE)",
        .file = "bm2dx.exe",
        .profile_slug = "iidx11",
        .size = 860160,
        .crc = 0x4EB98F3BU,
    },
    Build{
        .id = "iidx11",
        .name = "IIDX RED (E01 JAA)",
        .file = "bm2dx.exe",
        .profile_slug = "iidx11",
        .size = 1032192,
        .crc = 0x63423A04U,
    },
    Build{
        .id = "iidx12",
        .name = "IIDX 12 HAPPY SKY (JAD)",
        .file = "bm2dx.exe",
        .profile_slug = "iidx12",
        .size = 1105920,
        .crc = 0x122D2CC3U,
    },
};

std::array<std::uint32_t, 256> MakeCrcTable() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256; i++) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = ((c & 1U) != 0U) ? (0xEDB88320U ^ (c >> 1U)) : (c >> 1U);
        table[i] = c;
    }
    return table;
}

std::vector<std::uint8_t> ReadFile(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

void CollectCandidates(const std::filesystem::path& root, const std::string& name, int depth,
                       std::vector<std::filesystem::path>& out) {
    std::error_code ec;
    if (depth < 0) return;
    for (const auto& e : std::filesystem::directory_iterator(root, ec)) {
        if (ec) return;
        if (e.is_regular_file(ec) && e.path().filename().string() == name) out.push_back(e.path());
        if (e.is_directory(ec)) CollectCandidates(e.path(), name, depth - 1, out);
    }
}

std::uint32_t Crc32(const std::vector<std::uint8_t>& bytes) {
    static const std::array<std::uint32_t, 256> table = MakeCrcTable();
    std::uint32_t c = 0xFFFFFFFFU;
    for (const std::uint8_t b : bytes)
        c = table[(c ^ b) & 0xFFU] ^ (c >> 8U);
    return c ^ 0xFFFFFFFFU;
}

}

Match Identify(const std::string& game_dir) {
    Match match;
    if (game_dir.empty()) return match;
    const std::filesystem::path root(game_dir);
    const Build* by_size = nullptr;
    std::string by_size_file;

    for (const auto& build : kBuilds) {
        std::vector<std::filesystem::path> candidates;
        CollectCandidates(root, build.file, kSearchDepth, candidates);
        for (const auto& path : candidates) {
            std::error_code ec;
            const auto size = (std::uint64_t)std::filesystem::file_size(path, ec);
            if (ec || size != build.size) continue;
            if (Crc32(ReadFile(path)) == build.crc) {
                match.build = &build;
                match.file = path.string();
                return match;
            }
            if (by_size == nullptr) {
                by_size = &build;
                by_size_file = path.string();
            }
        }
    }

    if (by_size != nullptr) {
        LOG("Fingerprint", "%s matches %s by size but not by CRC - treating as a patched copy",
            by_size_file.c_str(), by_size->name);
        match.build = by_size;
        match.file = by_size_file;
    }
    return match;
}

const char* ProfileSlugFor(const std::string& build_id) {
    for (const Build& build : kBuilds) {
        if (build_id == build.id) return build.profile_slug;
    }
    return nullptr;
}

}
