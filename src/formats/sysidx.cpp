#include "formats/sysidx.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SysIdx {

namespace {

constexpr size_t kPathSlotBytes = 32;
constexpr size_t kHeaderPaths = 0x014;
constexpr size_t kCellTableFixed = 0x1B8;
constexpr size_t kRecordBytes = 36;
constexpr size_t kCellBytes = 8;
constexpr size_t kTailGapBytes = 44;

uint16_t U16(std::span<const uint8_t> b, size_t o) {
    if (o + 1 >= b.size()) return 0;
    return (uint16_t)((uint32_t)b[o] | ((uint32_t)b[o + 1] << 8));
}

int16_t I16(std::span<const uint8_t> b, size_t o) {
    return (int16_t)U16(b, o);
}

uint32_t U32(std::span<const uint8_t> b, size_t o) {
    if (o + 3 >= b.size()) return 0;
    return (uint32_t)b[o] | ((uint32_t)b[o + 1] << 8) | ((uint32_t)b[o + 2] << 16) |
           ((uint32_t)b[o + 3] << 24);
}

std::string FixedString(std::span<const uint8_t> b, size_t o, size_t max_len) {
    std::string s;
    for (size_t i = 0; i < max_len && o + i < b.size(); i++) {
        const uint8_t c = b[o + i];
        if (c == 0) break;
        s.push_back((char)c);
    }
    return s;
}

uint32_t U32Be(std::span<const uint8_t> b, size_t o) {
    if (o + 3 >= b.size()) return 0;
    return ((uint32_t)b[o] << 24U) | ((uint32_t)b[o + 1] << 16U) | ((uint32_t)b[o + 2] << 8U) |
           (uint32_t)b[o + 3];
}

bool IsSingleChunkBigEndian(std::span<const uint8_t> file) {
    return file.size() >= 8 && (size_t)U32Be(file, 0) + 4 == file.size();
}

bool SplitChunks(std::span<const uint8_t> file, std::span<const uint8_t>& chunk0,
                 std::span<const uint8_t>& chunk1, std::string& err) {
    if (file.size() < 8) {
        err = "index smaller than two chunk headers";
        return false;
    }
    const uint32_t size0 = U32(file, 0);
    if ((size_t)size0 + 8 > file.size()) {
        err = "chunk 0 size " + std::to_string(size0) + " runs past the file";
        return false;
    }
    const size_t c1_len_at = 4 + (size_t)size0;
    const uint32_t size1 = U32(file, c1_len_at);
    if (c1_len_at + 4 + (size_t)size1 != file.size()) {
        err = "chunk chain does not tile the file exactly (chunk0 " + std::to_string(size0) +
              " + chunk1 " + std::to_string(size1) + " != " + std::to_string(file.size()) + ")";
        return false;
    }
    chunk0 = file.subspan(4, size0);
    chunk1 = file.subspan(c1_len_at + 4, size1);
    return true;
}

void ReadKeys(std::span<const uint8_t> c0, uint32_t off, size_t stride, bool minus_one_only,
              std::vector<Key>& out) {
    out.clear();
    if (off == 0) return;
    size_t at = off;
    while (at + stride <= c0.size()) {
        const int16_t t = I16(c0, at);
        const bool done = minus_one_only ? (t == -1) : (t < 0);
        if (done) break;
        Key k;
        k.t = t;
        k.a = I16(c0, at + 2);
        if (stride >= 6) k.b = I16(c0, at + 4);
        out.push_back(k);
        at += stride;
    }
}

void ReadAlphaKeys(std::span<const uint8_t> c0, uint32_t off, std::vector<Key>& out) {
    out.clear();
    if (off == 0) return;
    size_t at = off;
    while (at + 4 <= c0.size()) {
        const int16_t t = I16(c0, at);
        if (t < 0) break;
        Key k;
        k.t = t;
        k.a = (int16_t)(int)(int8_t)(unsigned char)c0[at + 2];
        k.b = (int16_t)(int)(int8_t)(unsigned char)c0[at + 3];
        out.push_back(k);
        at += 4;
    }
}

void ReadRotationBlock(std::span<const uint8_t> c0, uint32_t off, std::vector<Key>& out) {
    out.clear();
    if (off == 0 || off + 12 > c0.size()) return;
    ReadKeys(c0, U32(c0, off), 4, false, out);
}

size_t ParseCells(std::span<const uint8_t> c0, uint32_t off, Package& out) {
    out.cells.clear();
    size_t at = off;
    while (at + kCellBytes <= c0.size()) {
        Cell c;
        c.x = U16(c0, at);
        c.y = U16(c0, at + 2);
        c.w = U16(c0, at + 4);
        c.h = U16(c0, at + 6);
        if (c.w == 0) break;
        out.cells.push_back(c);
        at += kCellBytes;
    }
    return at;
}

void ParseRecords(std::span<const uint8_t> c0, uint32_t off, Package& out) {
    out.records.clear();
    if (off == 0 || off >= c0.size()) return;
    const size_t count = (c0.size() - off) / kRecordBytes;
    out.records.reserve(count);
    for (size_t i = 0; i < count; i++) {
        const size_t at = off + (i * kRecordBytes);
        Record r;
        r.type = I16(c0, at);
        r.id = I16(c0, at + 2);
        r.flags = U16(c0, at + 4);
        r.duration = I16(c0, at + 6);
        r.t_start = I16(c0, at + 8);
        r.t_end = I16(c0, at + 10);
        r.t_base = I16(c0, at + 12);
        r.anchor_x = I16(c0, at + 16);
        r.anchor_y = I16(c0, at + 18);
        ReadKeys(c0, U32(c0, at + 20), 8, true, r.position);
        ReadKeys(c0, U32(c0, at + 24), 8, false, r.scale);
        ReadAlphaKeys(c0, U32(c0, at + 28), r.alpha);
        ReadRotationBlock(c0, U32(c0, at + 32), r.rotation);
        const bool last = r.type == kRecEndTable;
        out.records.push_back(std::move(r));
        if (last) break;
    }
}

size_t ParseRecordsEnd(std::span<const uint8_t> c0, uint32_t off) {
    if (off == 0 || off >= c0.size()) return 0;
    const size_t count = (c0.size() - off) / kRecordBytes;
    for (size_t i = 0; i < count; i++) {
        if (I16(c0, off + (i * kRecordBytes)) == kRecEndTable) {
            return off + ((i + 1) * kRecordBytes);
        }
    }
    return 0;
}

bool ParseSplitNameTable(std::span<const uint8_t> sec, size_t& at,
                         std::unordered_map<std::string, uint16_t>& out) {
    std::vector<std::string> names;
    while (at < sec.size()) {
        if (sec[at] == 0) {
            at++;
            break;
        }
        std::string name = FixedString(sec, at, sec.size() - at);
        at += name.size() + 1;
        names.push_back(std::move(name));
    }
    for (const auto& name : names) {
        if (at + 2 > sec.size()) return false;
        out.emplace(name, U16(sec, at));
        at += 2;
    }
    if (at < sec.size() && sec[at] == 0) at++;
    return !names.empty();
}

bool ParseNameTable(std::span<const uint8_t> c1, size_t& at,
                    std::unordered_map<std::string, uint16_t>& out) {
    while (at < c1.size()) {
        if (c1[at] == 0) {
            at++;
            return true;
        }
        const std::string name = FixedString(c1, at, c1.size() - at);
        at += name.size() + 1;
        if (at + 2 > c1.size()) return false;
        out.emplace(name, U16(c1, at));
        at += 2;
    }
    return false;
}

}

bool Parse(std::span<const uint8_t> file, Package& out, std::string& err) {
    out = Package{};

    std::span<const uint8_t> c0;
    std::span<const uint8_t> c1;
    const bool single_chunk = IsSingleChunkBigEndian(file);
    if (single_chunk) {
        c0 = file.subspan(4, U32Be(file, 0));
    } else if (!SplitChunks(file, c0, c1, err)) {
        return false;
    }
    if (c0.size() < kCellTableFixed) {
        err = "chunk 0 is smaller than the fixed header";
        return false;
    }

    out.texture_count = U16(c0, 0x002);
    const uint32_t off_cells = U32(c0, 0x004);
    const uint32_t off_records = U32(c0, 0x010);

    for (int i = 0; i < kMaxTextures; i++) {
        const std::string p =
            FixedString(c0, kHeaderPaths + ((size_t)i * kPathSlotBytes), kPathSlotBytes);
        if (p.empty()) break;
        out.texture_paths.push_back(p);
    }
    if (out.texture_paths.empty()) {
        err = "index lists no texture paths";
        return false;
    }

    if (off_cells >= c0.size()) {
        err = "cell table offset runs past chunk 0";
        return false;
    }
    const size_t cells_end = ParseCells(c0, off_cells, out);
    ParseRecords(c0, off_records, out);

    if (single_chunk) {
        const size_t records_end = ParseRecordsEnd(c0, off_records);
        const size_t len_at = (records_end != 0) ? records_end : (cells_end + kTailGapBytes);
        if (len_at + 4 > c0.size()) return true;
        const uint32_t len = U32(c0, len_at);
        if (len_at + 4 + (size_t)len != c0.size()) return true;

        const std::span<const uint8_t> section = c0.subspan(len_at + 4, len);
        size_t at = 0;
        ParseSplitNameTable(section, at, out.cell_names);
        ParseSplitNameTable(section, at, out.animation_names);
        return true;
    }

    size_t at = 0;
    std::unordered_map<std::string, uint16_t> unused_table;
    if (!ParseNameTable(c1, at, out.cell_names) || !ParseNameTable(c1, at, unused_table) ||
        !ParseNameTable(c1, at, out.animation_names)) {
        err = "name tables are truncated";
        return false;
    }
    return true;
}

int AnimationLength(const Package& pkg, size_t start_index) {
    for (size_t i = start_index; i < pkg.records.size(); i++) {
        if (pkg.records[i].type >= 0) continue;
        return pkg.records[i].t_end;
    }
    return 0;
}

}
