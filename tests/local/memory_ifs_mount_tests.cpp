#include <catch2/catch_test_macros.hpp>

#include "avs_boot.h"
#include "avs_funcs.h"
#include "formats/ifs_archive.h"
#include "support/dll_loader.h"
#include "support/env.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

namespace {

constexpr const char* kPackage = "/data/graphic/02005.ifs";
constexpr const char* kMagicEntry = "magic";

std::vector<uint8_t> ReadHostFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<uint8_t> ReadAvsFile(const AvsFuncs& avs, const std::string& path) {
    const int fd = avs.avs_fs_open(path.c_str(), 1, 420);
    if (fd < 0) return {};
    std::vector<uint8_t> out(256);
    const int got = avs.avs_fs_read(fd, out.data(), static_cast<int>(out.size()));
    avs.avs_fs_close(fd);
    out.resize(got > 0 ? static_cast<std::size_t>(got) : 0U);
    return out;
}

std::vector<uint8_t> WithMagic(const std::vector<uint8_t>& ifs, const std::vector<uint8_t>& magic) {
    auto archive = Ifs::Read(ifs);
    REQUIRE(archive.has_value());
    const auto entry =
        std::ranges::find(archive->entries, std::string(kMagicEntry), &Ifs::Entry::name);
    REQUIRE(entry != archive->entries.end());
    entry->bytes = magic;
    auto written = Ifs::Write(*archive);
    REQUIRE(written.has_value());
    return *written;
}

}

TEST_CASE("An IFS held in memory mounts through ramfs and imagefs, and remounts with new bytes") {
    const std::string dir = Support::EnvVar("R573_IIDX_DIR").value_or("");
    if (dir.empty()) SKIP("R573_IIDX_DIR not set");

    DllLoader avs_dll;
    REQUIRE(avs_dll.Load((dir + "/modules/avs2-core.dll").c_str()));
    AvsFuncs avs;
    REQUIRE(avs.Load(avs_dll));
    REQUIRE(avs.avs_filesys_ramfs != nullptr);
    REQUIRE(AvsManager::Boot(avs));

    const std::vector<uint8_t> original = ReadHostFile(dir + kPackage);
    REQUIRE(!original.empty());

    const AvsManager::MemoryIfs first{
        .bytes = original, .ramfs_mountpoint = "/memory_ifs/first", .mountpoint = "/memory_pkg"};
    REQUIRE(AvsManager::MountMemoryIfs(avs, first));
    CHECK(ReadAvsFile(avs, "/memory_pkg/magic") == std::vector<uint8_t>{'N', 'G', 'P', 'F'});
    AvsManager::UnmountMemoryIfs(avs, first);
    CHECK(ReadAvsFile(avs, "/memory_pkg/magic").empty());

    const AvsManager::MemoryIfs second{.bytes = WithMagic(original, {'F', 'P', 'G', 'N'}),
                                       .ramfs_mountpoint = "/memory_ifs/second",
                                       .mountpoint = "/memory_pkg"};
    REQUIRE(AvsManager::MountMemoryIfs(avs, second));
    CHECK(ReadAvsFile(avs, "/memory_pkg/magic") == std::vector<uint8_t>{'F', 'P', 'G', 'N'});
    AvsManager::UnmountMemoryIfs(avs, second);
}

TEST_CASE("Mounting an IFS from memory reports a build without ramfs instead of crashing") {
    AvsFuncs avs;
    const AvsManager::MemoryIfs ifs{
        .bytes = {1, 2, 3, 4}, .ramfs_mountpoint = "/memory_ifs/none", .mountpoint = "/memory_pkg"};
    CHECK_FALSE(AvsManager::MountMemoryIfs(avs, ifs));
}
