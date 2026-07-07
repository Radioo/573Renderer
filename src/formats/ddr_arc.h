#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace DdrArc {

constexpr uint32_t kMagic = 0x19751120U;

struct Entry {
    std::string name;
    uint32_t name_offset = 0;
    uint32_t data_offset = 0;
    uint32_t decomp_size = 0;
    uint32_t comp_len = 0;
    [[nodiscard]] bool stored() const { return comp_len >= decomp_size; }
};

struct Toc {
    uint32_t version = 0;
    uint32_t comp_flag = 0;
    std::vector<Entry> entries;
    [[nodiscard]] bool HasIfs() const;
};

[[nodiscard]] std::vector<uint8_t> Lz77Decompress(std::span<const uint8_t> src,
                                                  std::size_t expected_size);

[[nodiscard]] bool ParseToc(std::span<const uint8_t> data, Toc& out);

[[nodiscard]] bool ReadToc(const std::string& path, Toc& out);

[[nodiscard]] std::vector<uint8_t> DecompressEntry(std::span<const uint8_t> file,
                                                   const Entry& entry);

[[nodiscard]] std::vector<uint8_t> ExtractFirstIfs(const std::string& path, std::string& out_name);

}
