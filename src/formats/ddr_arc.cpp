#include "formats/ddr_arc.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace DdrArc {

namespace {

constexpr uint32_t kMaxEntries = 1000000U;
constexpr std::size_t kHeaderSize = 16;
constexpr std::size_t kEntrySize = 16;

uint32_t ReadU32LE(std::span<const uint8_t> data, std::size_t off) {
    return static_cast<uint32_t>(data[off]) | (static_cast<uint32_t>(data[off + 1]) << 8U) |
           (static_cast<uint32_t>(data[off + 2]) << 16U) |
           (static_cast<uint32_t>(data[off + 3]) << 24U);
}

char ToLowerAscii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool EndsWithCI(const std::string& s, std::string_view suffix) {
    if (s.size() < suffix.size()) return false;
    const std::size_t base = s.size() - suffix.size();
    for (std::size_t i = 0; i < suffix.size(); i++) {
        if (ToLowerAscii(s[base + i]) != ToLowerAscii(suffix[i])) return false;
    }
    return true;
}

std::string ReadName(std::span<const uint8_t> head, uint32_t off) {
    if (off >= head.size()) return std::format("<name@0x{:X}>", off);
    std::size_t end = off;
    while (end < head.size() && head[end] != 0)
        end++;
    std::string name;
    name.reserve(end - off);
    for (std::size_t i = off; i < end; i++)
        name.push_back(static_cast<char>(head[i]));
    return name;
}

void ParseEntries(std::span<const uint8_t> table, std::span<const uint8_t> names, uint32_t count,
                  Toc& out) {
    out.entries.clear();
    out.entries.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        const std::size_t off = static_cast<std::size_t>(i) * kEntrySize;
        Entry ent;
        ent.name_offset = ReadU32LE(table, off);
        ent.data_offset = ReadU32LE(table, off + 4);
        ent.decomp_size = ReadU32LE(table, off + 8);
        ent.comp_len = ReadU32LE(table, off + 12);
        ent.name = ReadName(names, ent.name_offset);
        out.entries.push_back(std::move(ent));
    }
}

struct FileCloser {
    void operator()(std::FILE* f) const { std::fclose(f); }
};
using FilePtr = std::unique_ptr<std::FILE, FileCloser>;

FilePtr OpenBinary(const std::string& path) {
    std::FILE* raw = nullptr;
    if (fopen_s(&raw, path.c_str(), "rb") != 0) return {};
    return FilePtr(raw);
}

bool ReadExact(std::FILE* f, std::span<uint8_t> into) {
    return std::fread(into.data(), 1, into.size(), f) == into.size();
}

bool SeekTo(std::FILE* f, uint64_t offset) {
    return _fseeki64(f, static_cast<long long>(offset), SEEK_SET) == 0;
}

}

bool Toc::HasIfs() const {
    return std::ranges::any_of(entries, [](const Entry& e) { return EndsWithCI(e.name, ".ifs"); });
}

std::vector<uint8_t> Lz77Decompress(std::span<const uint8_t> src, std::size_t expected_size) {
    std::vector<uint8_t> out;
    if (expected_size != 0) out.reserve(expected_size);

    std::array<uint8_t, 0x1000> window{};
    uint32_t pos = 0xFEE;

    std::size_t si = 0;
    uint32_t flags = 0;
    int flagbits = 0;

    while (si < src.size()) {
        if (flagbits == 0) {
            flags = src[si++];
            flagbits = 8;
            if (si >= src.size()) break;
        }
        const bool literal = (flags & 1U) != 0;
        flags >>= 1U;
        flagbits--;

        if (literal) {
            const uint8_t b = src[si++];
            out.push_back(b);
            window.at(pos) = b;
            pos = (pos + 1) & 0xFFFU;
        } else {
            if (si + 1 >= src.size()) break;
            const uint32_t token = (static_cast<uint32_t>(src[si]) << 8U) | src[si + 1];
            si += 2;
            const uint32_t distance = token >> 4U;
            if (distance == 0) break;
            const uint32_t length = (token & 0xFU) + 3;
            const uint32_t from = (pos - distance) & 0xFFFU;
            for (uint32_t i = 0; i < length; i++) {
                const uint8_t c = window.at((from + i) & 0xFFFU);
                out.push_back(c);
                window.at(pos) = c;
                pos = (pos + 1) & 0xFFFU;
            }
        }
        if (expected_size != 0 && out.size() >= expected_size) break;
    }
    return out;
}

bool ParseToc(std::span<const uint8_t> data, Toc& out) {
    if (data.size() < kHeaderSize) return false;
    if (ReadU32LE(data, 0) != kMagic) return false;
    const uint32_t count = ReadU32LE(data, 8);
    if (count == 0 || count > kMaxEntries) return false;
    const std::size_t table_end = kHeaderSize + (static_cast<std::size_t>(count) * kEntrySize);
    if (table_end > data.size()) return false;

    out.version = ReadU32LE(data, 4);
    out.comp_flag = ReadU32LE(data, 12);
    ParseEntries(data.subspan(kHeaderSize, static_cast<std::size_t>(count) * kEntrySize), data,
                 count, out);
    return true;
}

bool ReadToc(const std::string& path, Toc& out) {
    const FilePtr f = OpenBinary(path);
    if (f == nullptr) return false;

    std::array<uint8_t, kHeaderSize> hdr{};
    if (!ReadExact(f.get(), hdr)) return false;
    if (ReadU32LE(hdr, 0) != kMagic) return false;
    const uint32_t count = ReadU32LE(hdr, 8);
    if (count == 0 || count > kMaxEntries) return false;

    std::vector<uint8_t> table(static_cast<std::size_t>(count) * kEntrySize);
    if (!ReadExact(f.get(), table)) return false;

    uint32_t min_data = 0;
    for (uint32_t i = 0; i < count; i++) {
        const uint32_t d = ReadU32LE(table, (static_cast<std::size_t>(i) * kEntrySize) + 4);
        if (d != 0 && (min_data == 0 || d < min_data)) min_data = d;
    }
    const auto table_end =
        static_cast<uint32_t>(kHeaderSize + (static_cast<std::size_t>(count) * kEntrySize));
    const uint32_t names_end = (min_data != 0) ? min_data : table_end;

    std::vector<uint8_t> head(names_end);
    if (!SeekTo(f.get(), 0)) return false;
    const std::size_t got = std::fread(head.data(), 1, head.size(), f.get());
    head.resize(got);

    out.version = ReadU32LE(hdr, 4);
    out.comp_flag = ReadU32LE(hdr, 12);
    ParseEntries(table, head, count, out);
    return true;
}

std::vector<uint8_t> DecompressEntry(std::span<const uint8_t> file, const Entry& entry) {
    const std::size_t need = entry.stored() ? entry.decomp_size : entry.comp_len;
    if (static_cast<std::size_t>(entry.data_offset) + need > file.size()) return {};
    const std::span<const uint8_t> raw = file.subspan(entry.data_offset, need);
    if (entry.stored()) return {raw.begin(), raw.end()};
    return Lz77Decompress(raw, entry.decomp_size);
}

std::vector<uint8_t> ExtractFirstIfs(const std::string& path, std::string& out_name) {
    Toc toc;
    if (!ReadToc(path, toc)) return {};

    const Entry* hit = nullptr;
    for (const Entry& e : toc.entries) {
        if (EndsWithCI(e.name, ".ifs")) {
            hit = &e;
            break;
        }
    }
    if (hit == nullptr) return {};
    out_name = hit->name;

    const FilePtr f = OpenBinary(path);
    if (f == nullptr) return {};
    const std::size_t need = hit->stored() ? hit->decomp_size : hit->comp_len;
    std::vector<uint8_t> raw(need);
    if (!SeekTo(f.get(), hit->data_offset)) return {};
    if (!ReadExact(f.get(), raw)) return {};

    if (hit->stored()) return raw;
    return Lz77Decompress(raw, hit->decomp_size);
}

}
