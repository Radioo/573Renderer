#include "formats/txp2.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>
#include <string>
#include <vector>

namespace Txp2 {

namespace {

constexpr uint32_t kMagicBe = 0x54585032U;
constexpr uint32_t kFixedHeaderBytes = 24;

constexpr uint32_t kFlagTextures = 0x00001U;
constexpr uint32_t kFlagTextureNames = 0x00002U;
constexpr uint32_t kFlagLegacyLz = 0x00004U;
constexpr uint32_t kFlagCells = 0x00008U;
constexpr uint32_t kFlagCellNames = 0x00010U;
constexpr uint32_t kFlagNameScramble = 0x00020U;
constexpr uint32_t kFlagRecords40 = 0x00040U;
constexpr uint32_t kFlagRecords40Names = 0x00080U;
constexpr uint32_t kFlagRecords100 = 0x00100U;
constexpr uint32_t kFlagRecords100Names = 0x00200U;
constexpr uint32_t kFlagCoreSize = 0x00400U;
constexpr uint32_t kFlagAfpStreams = 0x00800U;
constexpr uint32_t kFlagAfpNames = 0x01000U;
constexpr uint32_t kFlagGeometry = 0x02000U;
constexpr uint32_t kFlagGeometryNames = 0x04000U;
constexpr uint32_t kFlagObject8000 = 0x08000U;
constexpr uint32_t kFlagFontLib = 0x10000U;
constexpr uint32_t kFlagAppendedBlock = 0x20000U;
constexpr uint32_t kFlagTextureLz = 0x40000U;

struct SectionSpec {
    uint32_t bit;
    uint32_t dwords;
};

constexpr std::array<SectionSpec, 16> kSections = {{
    {.bit = kFlagTextures, .dwords = 2},
    {.bit = kFlagTextureNames, .dwords = 1},
    {.bit = kFlagCells, .dwords = 2},
    {.bit = kFlagCellNames, .dwords = 1},
    {.bit = kFlagRecords40, .dwords = 2},
    {.bit = kFlagRecords40Names, .dwords = 1},
    {.bit = kFlagRecords100, .dwords = 2},
    {.bit = kFlagRecords100Names, .dwords = 1},
    {.bit = kFlagCoreSize, .dwords = 1},
    {.bit = kFlagAfpStreams, .dwords = 2},
    {.bit = kFlagAfpNames, .dwords = 1},
    {.bit = kFlagGeometry, .dwords = 2},
    {.bit = kFlagGeometryNames, .dwords = 1},
    {.bit = kFlagObject8000, .dwords = 1},
    {.bit = kFlagFontLib, .dwords = 1},
    {.bit = kFlagAppendedBlock, .dwords = 1},
}};

struct Sections {
    uint32_t tex_count = 0;
    uint32_t tex_array = 0;
    uint32_t cell_count = 0;
    uint32_t cell_array = 0;
    uint32_t cell_names = 0;
    uint32_t afp_count = 0;
    uint32_t afp_array = 0;
    uint32_t geo_count = 0;
    uint32_t geo_array = 0;
};

constexpr size_t kNameEntryBytes = 12;
constexpr size_t kGeoEntryBytes = 12;
constexpr size_t kGeoPrimBytes = 16;
constexpr size_t kGeoBodyVertexCount = 20;
constexpr size_t kGeoBodyUvCount = 22;
constexpr size_t kGeoBodyColorCount = 24;
constexpr size_t kGeoBodyRefCount = 26;
constexpr size_t kGeoBodyPrimCount = 28;
constexpr size_t kGeoBodyPositions = 32;
constexpr size_t kGeoBodyUvs = 36;
constexpr size_t kGeoBodyColors = 40;
constexpr size_t kGeoBodyRefs = 44;
constexpr size_t kGeoBodyPrims = 48;

class Reader {
public:
    Reader(std::span<const uint8_t> buf, bool big_endian) : buf_(buf), be_(big_endian) {}

    [[nodiscard]] bool InRange(size_t off, size_t len) const {
        return off <= buf_.size() && len <= buf_.size() - off;
    }

    [[nodiscard]] uint32_t U32(size_t off) const {
        if (!InRange(off, 4)) return 0;
        const auto b0 = static_cast<uint32_t>(buf_[off]);
        const auto b1 = static_cast<uint32_t>(buf_[off + 1]);
        const auto b2 = static_cast<uint32_t>(buf_[off + 2]);
        const auto b3 = static_cast<uint32_t>(buf_[off + 3]);
        if (be_) return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
        return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
    }

    [[nodiscard]] uint16_t U16(size_t off) const {
        if (!InRange(off, 2)) return 0;
        const auto b0 = static_cast<uint16_t>(buf_[off]);
        const auto b1 = static_cast<uint16_t>(buf_[off + 1]);
        if (be_) return static_cast<uint16_t>((b0 << 8) | b1);
        return static_cast<uint16_t>((b1 << 8) | b0);
    }

    [[nodiscard]] uint8_t U8(size_t off) const {
        if (!InRange(off, 1)) return 0;
        return buf_[off];
    }

    [[nodiscard]] float F32(size_t off) const {
        const uint32_t bits = U32(off);
        float out = 0.0F;
        static_assert(sizeof(out) == sizeof(bits));
        std::memcpy(&out, &bits, sizeof(out));
        return out;
    }

    [[nodiscard]] std::string Str(size_t off, bool descramble) const {
        if (off == 0 || off >= buf_.size()) return {};
        std::string text;
        for (size_t i = off; i < buf_.size() && buf_[i] != 0; i++)
            text.push_back(static_cast<char>(buf_[i]));
        if (descramble) Descramble(text);
        return text;
    }

private:
    static void Descramble(std::string& s) {
        if (s.empty()) return;
        const auto first = static_cast<unsigned char>(s[0]);
        if (first >= 0x20 && first < 0xA0) return;
        for (char& c : s)
            c = static_cast<char>(static_cast<unsigned char>(c) + 0x80);
    }

    std::span<const uint8_t> buf_;
    bool be_;
};

}

namespace {

bool DecodeTables(const Reader& r, const Sections& sec, bool descramble, Package& out,
                  std::string& err);
bool DecodeAfpTable(const Reader& r, const Sections& sec, bool descramble, Package& out,
                    std::string& err);
bool DecodeTextureTable(const Reader& r, const Sections& sec, bool descramble, Package& out,
                        std::string& err);
bool DecodeCellTables(const Reader& r, const Sections& sec, bool descramble, Package& out,
                      std::string& err);
bool DecodeGeoTable(const Reader& r, const Sections& sec, bool descramble, Package& out,
                    std::string& err);

size_t WalkSections(const Reader& r, Package& out, Sections& sec) {
    size_t cursor = kFixedHeaderBytes;
    for (const auto& spec : kSections) {
        if ((out.flags & spec.bit) == 0) continue;
        const uint32_t a = r.U32(cursor);
        const uint32_t b = (spec.dwords == 2) ? r.U32(cursor + 4) : 0;
        cursor += static_cast<size_t>(spec.dwords) * 4;

        switch (spec.bit) {
        case kFlagTextures:
            sec.tex_count = a;
            sec.tex_array = b;
            break;
        case kFlagCells:
            sec.cell_count = a;
            sec.cell_array = b;
            break;
        case kFlagCellNames:
            sec.cell_names = a;
            break;
        case kFlagCoreSize:
            out.core_size = a;
            break;
        case kFlagAfpStreams:
            sec.afp_count = a;
            sec.afp_array = b;
            break;
        case kFlagGeometry:
            sec.geo_count = a;
            sec.geo_array = b;
            break;
        case kFlagAppendedBlock:
            out.appended_block_offset = a;
            break;
        default:
            break;
        }
    }
    return cursor;
}

}

uint32_t SectionDwordCount(uint32_t flag_bit) {
    for (const auto& s : kSections) {
        if (s.bit == flag_bit) return s.dwords;
    }
    return 0;
}

bool Parse(const std::vector<uint8_t>& core, Package& out, std::string& err) {
    if (core.size() < kFixedHeaderBytes) {
        err = "package smaller than the 24-byte fixed header";
        return false;
    }

    const std::span<const uint8_t> bytes(core);
    const uint32_t magic_be = (static_cast<uint32_t>(bytes[0]) << 24) |
                              (static_cast<uint32_t>(bytes[1]) << 16) |
                              (static_cast<uint32_t>(bytes[2]) << 8) | bytes[3];
    const uint32_t magic_le = (static_cast<uint32_t>(bytes[3]) << 24) |
                              (static_cast<uint32_t>(bytes[2]) << 16) |
                              (static_cast<uint32_t>(bytes[1]) << 8) | bytes[0];
    if (magic_be == kMagicBe) {
        out.big_endian = true;
    } else if (magic_le == kMagicBe) {
        out.big_endian = false;
    } else {
        err = "not TXP2 package data";
        return false;
    }

    const Reader r(bytes, out.big_endian);
    out.header_size = r.U32(16);
    out.flags = r.U32(20);
    out.legacy_lz = (out.flags & kFlagLegacyLz) != 0;
    out.names_obfuscated = (out.flags & kFlagNameScramble) != 0;
    out.textures_lz_compressed = (out.flags & kFlagTextureLz) != 0;

    if (out.legacy_lz) {
        err = "package uses the legacy LZ form, which the game itself rejects";
        return false;
    }

    Sections sec;
    const size_t cursor = WalkSections(r, out, sec);

    if (out.header_size != 0 && out.header_size != cursor) {
        err = "header size " + std::to_string(out.header_size) + " disagrees with the " +
              std::to_string(cursor) + " bytes the flag word describes";
        return false;
    }

    return DecodeTables(r, sec, out.names_obfuscated, out, err);
}

namespace {

bool DecodeAfpTable(const Reader& r, const Sections& sec, bool descramble, Package& out,
                    std::string& err) {
    out.afp_entries.clear();
    out.afp_entries.reserve(sec.afp_count);
    for (uint32_t i = 0; i < sec.afp_count; i++) {
        const size_t e = (size_t)sec.afp_array + ((size_t)i * 12);
        if (!r.InRange(e, 12)) {
            err = "afp stream table runs past the package";
            return false;
        }
        AfpEntry entry;
        entry.name = r.Str(r.U32(e), descramble);
        entry.size = r.U32(e + 4);
        entry.data_offset = r.U32(e + 8);
        out.afp_entries.push_back(std::move(entry));
    }

    return true;
}

bool DecodeTextureTable(const Reader& r, const Sections& sec, bool descramble, Package& out,
                        std::string& err) {
    out.textures.clear();
    out.textures.reserve(sec.tex_count);
    for (uint32_t i = 0; i < sec.tex_count; i++) {
        const size_t e = (size_t)sec.tex_array + ((size_t)i * 12);
        if (!r.InRange(e, 12)) {
            err = "texture table runs past the package";
            return false;
        }
        TextureEntry entry;
        entry.name = r.Str(r.U32(e), descramble);
        entry.size = r.U32(e + 4);
        entry.file_offset = r.U32(e + 8);
        out.textures.push_back(std::move(entry));
    }

    return true;
}

bool DecodeCellTables(const Reader& r, const Sections& sec, bool descramble, Package& out,
                      std::string& err) {
    out.cells.clear();
    out.cells.reserve(sec.cell_count);
    for (uint32_t i = 0; i < sec.cell_count; i++) {
        const size_t e = (size_t)sec.cell_array + ((size_t)i * 10);
        if (!r.InRange(e, 10)) {
            err = "cell table runs past the package";
            return false;
        }
        CellEntry cell;
        cell.texture_index = r.U16(e);
        cell.x0 = r.U16(e + 2);
        cell.y0 = r.U16(e + 4);
        cell.x1 = r.U16(e + 6);
        cell.y1 = r.U16(e + 8);
        out.cells.push_back(cell);
    }

    out.cell_names.clear();
    if (sec.cell_names != 0 && r.InRange(sec.cell_names, 28)) {
        const uint32_t n = r.U32((size_t)sec.cell_names + 16);
        const uint32_t arr = r.U32((size_t)sec.cell_names + 24);
        out.cell_names.reserve(n);
        for (uint32_t i = 0; i < n; i++) {
            const size_t e = (size_t)arr + ((size_t)i * kNameEntryBytes);
            if (!r.InRange(e, kNameEntryBytes)) break;
            out.cell_names.push_back(
                {.name = r.Str(r.U32(e + 8), descramble), .cell_index = r.U32(e + 4)});
        }
    }

    return true;
}

}

namespace {

size_t GeoRelocate(const Reader& r, size_t body, size_t field) {
    const uint32_t stored = r.U32(body + field);
    return stored == 0 ? 0 : body + stored;
}

void DecodeGeoFloatPairs(const Reader& r, size_t base, uint16_t count, std::vector<float>& out) {
    out.clear();
    if (base == 0 || count == 0) return;
    out.reserve((size_t)count * 2);
    for (uint16_t i = 0; i < count; i++) {
        const size_t e = base + ((size_t)i * 8);
        if (!r.InRange(e, 8)) break;
        out.push_back(r.F32(e));
        out.push_back(r.F32(e + 4));
    }
}

void DecodeGeoRefs(const Reader& r, size_t body, uint16_t count, bool descramble,
                   std::vector<std::string>& out) {
    out.clear();
    const size_t base = GeoRelocate(r, body, kGeoBodyRefs);
    if (base == 0 || count == 0) return;
    out.reserve(count);
    for (uint16_t i = 0; i < count; i++) {
        const size_t e = base + ((size_t)i * 4);
        if (!r.InRange(e, 4)) break;
        const uint32_t stored = r.U32(e);
        out.push_back(stored == 0 ? std::string() : r.Str(body + stored, descramble));
    }
}

void DecodeGeoPrims(const Reader& r, size_t body, uint16_t count, std::vector<GeoPrim>& out) {
    out.clear();
    const size_t base = GeoRelocate(r, body, kGeoBodyPrims);
    if (base == 0 || count == 0) return;
    out.reserve(count);
    for (uint16_t i = 0; i < count; i++) {
        const size_t e = base + ((size_t)i * kGeoPrimBytes);
        if (!r.InRange(e, kGeoPrimBytes)) break;
        GeoPrim prim;
        prim.flags = r.U8(e + 1);
        prim.bitmap_ref = r.U8(e + 2);
        prim.index_count = r.U16(e + 4);
        for (size_t c = 0; c < prim.rgba.size(); c++)
            prim.rgba.at(c) = r.U8(e + 8 + c);
        const uint32_t stored = r.U32(e + 12);
        if (stored != 0) {
            const size_t idx = body + stored;
            prim.indices.reserve(prim.index_count);
            for (uint16_t k = 0; k < prim.index_count; k++) {
                if (!r.InRange(idx + ((size_t)k * 2), 2)) break;
                prim.indices.push_back(r.U16(idx + ((size_t)k * 2)));
            }
        }
        out.push_back(std::move(prim));
    }
}

void DecodeGeoBody(const Reader& r, size_t body, bool descramble, GeoShape& shape) {
    shape.vertex_count = r.U16(body + kGeoBodyVertexCount);
    shape.uv_count = r.U16(body + kGeoBodyUvCount);
    shape.color_count = r.U16(body + kGeoBodyColorCount);
    const uint16_t ref_count = r.U16(body + kGeoBodyRefCount);
    const uint16_t prim_count = r.U16(body + kGeoBodyPrimCount);

    DecodeGeoFloatPairs(r, GeoRelocate(r, body, kGeoBodyPositions), shape.vertex_count,
                        shape.positions);
    DecodeGeoFloatPairs(r, GeoRelocate(r, body, kGeoBodyUvs), shape.uv_count, shape.uvs);

    shape.colors.clear();
    const size_t colors = GeoRelocate(r, body, kGeoBodyColors);
    if (colors != 0 && shape.color_count != 0) {
        shape.colors.reserve(shape.color_count);
        for (uint16_t i = 0; i < shape.color_count; i++)
            shape.colors.push_back(r.U8(colors + i));
    }

    DecodeGeoRefs(r, body, ref_count, descramble, shape.bitmap_names);
    DecodeGeoPrims(r, body, prim_count, shape.prims);
}

bool DecodeGeoTable(const Reader& r, const Sections& sec, bool descramble, Package& out,
                    std::string& err) {
    out.shapes.clear();
    out.shapes.reserve(sec.geo_count);
    for (uint32_t i = 0; i < sec.geo_count; i++) {
        const size_t e = (size_t)sec.geo_array + ((size_t)i * kGeoEntryBytes);
        if (!r.InRange(e, kGeoEntryBytes)) {
            err = "geometry table runs past the package";
            return false;
        }
        GeoShape shape;
        shape.name = r.Str(r.U32(e), descramble);
        const uint32_t body = r.U32(e + 8);
        if (body != 0 && r.InRange(body, kGeoBodyPrims + 4))
            DecodeGeoBody(r, body, descramble, shape);
        out.shapes.push_back(std::move(shape));
    }
    return true;
}

bool DecodeTables(const Reader& r, const Sections& sec, bool descramble, Package& out,
                  std::string& err) {
    if (!DecodeAfpTable(r, sec, descramble, out, err)) return false;
    if (!DecodeTextureTable(r, sec, descramble, out, err)) return false;
    if (!DecodeCellTables(r, sec, descramble, out, err)) return false;
    return DecodeGeoTable(r, sec, descramble, out, err);
}

}

}
