#include "formats/ddr_arc.h"

#include "formats/avs_lz77.h"
#include "formats/little_endian.h"

#include "support/expected.h"

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
        ent.name_offset = LittleEndian::ReadU32(table, off);
        ent.data_offset = LittleEndian::ReadU32(table, off + 4);
        ent.decomp_size = LittleEndian::ReadU32(table, off + 8);
        ent.comp_len = LittleEndian::ReadU32(table, off + 12);
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

bool ParseToc(std::span<const uint8_t> data, Toc& out) {
    if (data.size() < kHeaderSize) return false;
    if (LittleEndian::ReadU32(data, 0) != kMagic) return false;
    const uint32_t count = LittleEndian::ReadU32(data, 8);
    if (count == 0 || count > kMaxEntries) return false;
    const std::size_t table_end = kHeaderSize + (static_cast<std::size_t>(count) * kEntrySize);
    if (table_end > data.size()) return false;

    out.version = LittleEndian::ReadU32(data, 4);
    out.comp_flag = LittleEndian::ReadU32(data, 12);
    ParseEntries(data.subspan(kHeaderSize, static_cast<std::size_t>(count) * kEntrySize), data,
                 count, out);
    return true;
}

Support::Expected<Toc, std::string> ReadToc(const std::string& path) {
    const FilePtr f = OpenBinary(path);
    if (f == nullptr) return Support::Unexpected("cannot open: " + path);

    std::array<uint8_t, kHeaderSize> hdr{};
    if (!ReadExact(f.get(), hdr)) return Support::Unexpected("truncated header: " + path);
    if (LittleEndian::ReadU32(hdr, 0) != kMagic) return Support::Unexpected("bad magic: " + path);
    const uint32_t count = LittleEndian::ReadU32(hdr, 8);
    if (count == 0 || count > kMaxEntries)
        return Support::Unexpected("entry count out of range: " + path);

    std::vector<uint8_t> table(static_cast<std::size_t>(count) * kEntrySize);
    if (!ReadExact(f.get(), table)) return Support::Unexpected("truncated entry table: " + path);

    uint32_t min_data = 0;
    for (uint32_t i = 0; i < count; i++) {
        const uint32_t d =
            LittleEndian::ReadU32(table, (static_cast<std::size_t>(i) * kEntrySize) + 4);
        if (d != 0 && (min_data == 0 || d < min_data)) min_data = d;
    }
    const auto table_end =
        static_cast<uint32_t>(kHeaderSize + (static_cast<std::size_t>(count) * kEntrySize));
    const uint32_t names_end = (min_data != 0) ? min_data : table_end;

    std::vector<uint8_t> head(names_end);
    if (!SeekTo(f.get(), 0)) return Support::Unexpected("seek failed: " + path);
    const std::size_t got = std::fread(head.data(), 1, head.size(), f.get());
    head.resize(got);

    Toc out;
    out.version = LittleEndian::ReadU32(hdr, 4);
    out.comp_flag = LittleEndian::ReadU32(hdr, 12);
    ParseEntries(table, head, count, out);
    return out;
}

std::vector<uint8_t> DecompressEntry(std::span<const uint8_t> file, const Entry& entry) {
    const std::size_t need = entry.stored() ? entry.decomp_size : entry.comp_len;
    if (static_cast<std::size_t>(entry.data_offset) + need > file.size()) return {};
    const std::span<const uint8_t> raw = file.subspan(entry.data_offset, need);
    if (entry.stored()) return {raw.begin(), raw.end()};
    return AvsLz77::Decompress(raw, entry.decomp_size);
}

Support::Expected<std::vector<uint8_t>, std::string> ExtractFirstIfs(const std::string& path,
                                                                     std::string& out_name) {
    auto toc = ReadToc(path);
    if (!toc) return Support::Unexpected(std::move(toc).error());

    const Entry* hit = nullptr;
    for (const Entry& e : toc->entries) {
        if (EndsWithCI(e.name, ".ifs")) {
            hit = &e;
            break;
        }
    }
    if (hit == nullptr) return Support::Unexpected("no .ifs entry in " + path);
    out_name = hit->name;

    const FilePtr f = OpenBinary(path);
    if (f == nullptr) return Support::Unexpected("cannot reopen: " + path);
    const std::size_t need = hit->stored() ? hit->decomp_size : hit->comp_len;
    std::vector<uint8_t> raw(need);
    if (!SeekTo(f.get(), hit->data_offset))
        return Support::Unexpected("seek to entry failed: " + path);
    if (!ReadExact(f.get(), raw)) return Support::Unexpected("truncated entry data: " + path);

    if (hit->stored()) return raw;
    return AvsLz77::Decompress(raw, hit->decomp_size);
}

}
