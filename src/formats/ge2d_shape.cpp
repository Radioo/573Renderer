#include "formats/ge2d_shape.h"

#include "formats/big_endian.h"
#include "formats/little_endian.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Ge2dShape {

namespace {

constexpr uint32_t kMagic = 0x47453244;
constexpr std::array<uint8_t, 4> kBigEndianPackage = {0x4E, 0x47, 0x50, 0x46};

constexpr std::size_t kVersionField = 4;
constexpr std::size_t kUnreadValueField = 8;
constexpr std::size_t kSizeField = 12;
constexpr std::size_t kFlagsField = 16;
constexpr std::size_t kVertexCountField = 20;
constexpr std::size_t kUvCountField = 22;
constexpr std::size_t kColourCountField = 24;
constexpr std::size_t kNameCountField = 26;
constexpr std::size_t kPrimitiveCountField = 28;
constexpr std::size_t kUnreadWordField = 30;
constexpr std::size_t kVertexTableField = 32;
constexpr std::size_t kUvTableField = 36;
constexpr std::size_t kColourTableField = 40;
constexpr std::size_t kNameTableField = 44;
constexpr std::size_t kPrimitiveTableField = 48;
constexpr std::size_t kHeaderSize = 52;
constexpr std::size_t kRectSize = 16;
constexpr uint32_t kFlagRect = 0x4;

constexpr std::size_t kPointSize = 8;
constexpr std::size_t kFloatSize = 4;
constexpr std::size_t kColourSize = 4;
constexpr std::size_t kNameOffsetSize = 4;
constexpr std::size_t kIndexSize = 2;
constexpr std::size_t kPrimitiveSize = 16;
constexpr std::size_t kPrimitiveKindField = 0;
constexpr std::size_t kPrimitiveDrawFlagsField = 1;
constexpr std::size_t kPrimitiveTextureField = 2;
constexpr std::size_t kPrimitiveSecondTextureField = 3;
constexpr std::size_t kPrimitiveIndexCountField = 4;
constexpr std::size_t kPrimitiveUnreadBytesField = 6;
constexpr std::size_t kPrimitiveColourField = 8;
constexpr std::size_t kPrimitiveIndexTableField = 12;

constexpr std::size_t kMaxCount = 0xFFFF;
constexpr std::size_t kMaxFileSize = 0xFFFFFFFF;
constexpr std::size_t kAlignment = 4;

std::size_t AlignUp(std::size_t value) {
    return (value + kAlignment - 1) & ~(kAlignment - 1);
}

class Reader {
public:
    Reader(std::span<const uint8_t> bytes, ByteOrder order) : bytes_(bytes), order_(order) {}

    [[nodiscard]] bool Has(std::size_t off, std::size_t length) const {
        return off <= bytes_.size() && length <= bytes_.size() - off;
    }

    [[nodiscard]] uint16_t U16(std::size_t off) const {
        return order_ == ByteOrder::Big ? BigEndian::ReadU16(bytes_, off)
                                        : LittleEndian::ReadU16(bytes_, off);
    }

    [[nodiscard]] uint32_t U32(std::size_t off) const {
        return order_ == ByteOrder::Big ? BigEndian::ReadU32(bytes_, off)
                                        : LittleEndian::ReadU32(bytes_, off);
    }

    [[nodiscard]] std::span<const uint8_t> Bytes() const { return bytes_; }

private:
    std::span<const uint8_t> bytes_;
    ByteOrder order_;
};

class Writer {
public:
    Writer(std::size_t size, ByteOrder order) : data_(size, 0), order_(order) {}

    void U16(std::size_t off, uint16_t value) {
        if (order_ == ByteOrder::Big) {
            BigEndian::WriteU16(data_, off, value);
        } else {
            LittleEndian::WriteU16(data_, off, value);
        }
    }

    void U32(std::size_t off, uint32_t value) {
        if (order_ == ByteOrder::Big) {
            BigEndian::WriteU32(data_, off, value);
        } else {
            LittleEndian::WriteU32(data_, off, value);
        }
    }

    void Raw(std::size_t off, std::span<const uint8_t> bytes) {
        std::ranges::copy(bytes, data_.begin() + static_cast<std::ptrdiff_t>(off));
    }

    [[nodiscard]] std::vector<uint8_t> Take() && { return std::move(data_); }

private:
    std::vector<uint8_t> data_;
    ByteOrder order_;
};

struct TableFields {
    std::size_t count_field = 0;
    std::size_t table_field = 0;
    std::size_t element_size = 0;
    const char* name = "";
};

constexpr TableFields kVertexTable{.count_field = kVertexCountField,
                                   .table_field = kVertexTableField,
                                   .element_size = kPointSize,
                                   .name = "vertex"};
constexpr TableFields kUvTable{.count_field = kUvCountField,
                               .table_field = kUvTableField,
                               .element_size = kPointSize,
                               .name = "UV"};
constexpr TableFields kColourTable{.count_field = kColourCountField,
                                   .table_field = kColourTableField,
                                   .element_size = kColourSize,
                                   .name = "vertex colour"};
constexpr TableFields kNameTable{.count_field = kNameCountField,
                                 .table_field = kNameTableField,
                                 .element_size = kNameOffsetSize,
                                 .name = "texture name"};
constexpr TableFields kPrimitiveTable{.count_field = kPrimitiveCountField,
                                      .table_field = kPrimitiveTableField,
                                      .element_size = kPrimitiveSize,
                                      .name = "primitive"};

class Claims {
public:
    void Add(std::size_t off, std::size_t length) {
        if (length > 0) spans_.emplace_back(off, length);
    }

    [[nodiscard]] Support::Expected<void, std::string> Check(std::span<const uint8_t> bytes) {
        std::ranges::sort(spans_);
        std::size_t end = 0;
        for (const auto& [off, length] : spans_) {
            if (off < end)
                return Support::Unexpected(std::format("shape tables overlap at {}", off));
            if (!Zero(bytes.subspan(end, off - end))) {
                return Support::Unexpected(
                    std::format("bytes at {} belong to no shape table", end));
            }
            end = off + length;
        }
        if (!Zero(bytes.subspan(end)))
            return Support::Unexpected(std::format("bytes at {} belong to no shape table", end));
        return {};
    }

private:
    static bool Zero(std::span<const uint8_t> bytes) {
        return std::ranges::all_of(bytes, [](uint8_t b) { return b == 0; });
    }

    std::vector<std::pair<std::size_t, std::size_t>> spans_;
};

struct Table {
    std::size_t offset = 0;
    std::size_t count = 0;
};

Support::Expected<Table, std::string> LocateTable(const Reader& r, const TableFields& fields,
                                                  Claims& claims) {
    const Table table{.offset = r.U32(fields.table_field), .count = r.U16(fields.count_field)};
    if (table.count == 0) {
        if (table.offset != 0) {
            return Support::Unexpected(
                std::format("{} table has an offset but no entries", fields.name));
        }
        return table;
    }
    if (!r.Has(table.offset, table.count * fields.element_size))
        return Support::Unexpected(std::format("{} table lies outside the shape", fields.name));
    claims.Add(table.offset, table.count * fields.element_size);
    return table;
}

Support::Expected<std::vector<std::array<uint32_t, 2>>, std::string>
ReadPoints(const Reader& r, const TableFields& fields, Claims& claims) {
    const auto table = LocateTable(r, fields, claims);
    if (!table) return Support::Unexpected(table.error());
    std::vector<std::array<uint32_t, 2>> points(table->count);
    for (std::size_t i = 0; i < table->count; i++) {
        const std::size_t at = table->offset + (i * kPointSize);
        points[i] = {r.U32(at), r.U32(at + kFloatSize)};
    }
    return points;
}

Support::Expected<std::vector<std::array<uint8_t, 4>>, std::string> ReadColours(const Reader& r,
                                                                                Claims& claims) {
    const auto table = LocateTable(r, kColourTable, claims);
    if (!table) return Support::Unexpected(table.error());
    std::vector<std::array<uint8_t, 4>> colours(table->count);
    const auto bytes = r.Bytes();
    for (std::size_t i = 0; i < table->count; i++) {
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(table->offset + (i * kColourSize)),
                    kColourSize, colours[i].begin());
    }
    return colours;
}

Support::Expected<std::vector<std::string>, std::string> ReadNames(const Reader& r,
                                                                   Claims& claims) {
    const auto table = LocateTable(r, kNameTable, claims);
    if (!table) return Support::Unexpected(table.error());
    std::vector<std::string> names;
    for (std::size_t i = 0; i < table->count; i++) {
        const std::size_t start = r.U32(table->offset + (i * kNameOffsetSize));
        if (!r.Has(start, 0))
            return Support::Unexpected(std::format("texture name {} lies outside the shape", i));
        const auto rest = r.Bytes().subspan(start);
        const auto nul = std::ranges::find(rest, uint8_t{0});
        if (nul == rest.end())
            return Support::Unexpected(std::format("texture name {} has no terminator", i));
        names.emplace_back(rest.begin(), nul);
        claims.Add(start, names.back().size() + 1);
    }
    return names;
}

Support::Expected<std::vector<uint16_t>, std::string>
ReadIndices(const Reader& r, std::size_t record, Claims& claims) {
    const std::size_t count = r.U16(record + kPrimitiveIndexCountField);
    const std::size_t offset = r.U32(record + kPrimitiveIndexTableField);
    std::vector<uint16_t> indices;
    if (count == 0) {
        if (offset != 0)
            return Support::Unexpected(std::string("an empty index array has an offset"));
        return indices;
    }
    if (!r.Has(offset, count * kIndexSize))
        return Support::Unexpected(std::string("an index array lies outside the shape"));
    claims.Add(offset, count * kIndexSize);
    for (std::size_t j = 0; j < count; j++)
        indices.push_back(r.U16(offset + (j * kIndexSize)));
    return indices;
}

Support::Expected<std::vector<Primitive>, std::string> ReadPrimitives(const Reader& r,
                                                                      Claims& claims) {
    const auto table = LocateTable(r, kPrimitiveTable, claims);
    if (!table) return Support::Unexpected(table.error());
    const auto bytes = r.Bytes();
    std::vector<Primitive> primitives;
    for (std::size_t i = 0; i < table->count; i++) {
        const std::size_t record = table->offset + (i * kPrimitiveSize);
        Primitive primitive{.kind = bytes[record + kPrimitiveKindField],
                            .draw_flags = bytes[record + kPrimitiveDrawFlagsField],
                            .texture = bytes[record + kPrimitiveTextureField],
                            .second_texture = bytes[record + kPrimitiveSecondTextureField],
                            .unread_bytes = {bytes[record + kPrimitiveUnreadBytesField],
                                             bytes[record + kPrimitiveUnreadBytesField + 1]},
                            .colour = {},
                            .indices = {}};
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(record + kPrimitiveColourField),
                    kColourSize, primitive.colour.begin());
        auto indices = ReadIndices(r, record, claims);
        if (!indices)
            return Support::Unexpected(std::format("primitive {}: {}", i, indices.error()));
        primitive.indices = std::move(*indices);
        primitives.push_back(std::move(primitive));
    }
    return primitives;
}

Support::Expected<void, std::string> Validate(const Shape& shape) {
    if ((shape.flags & kFlagRect) != 0) {
        return Support::Unexpected(
            std::string("flags must not hold the rect bit; set rect instead"));
    }
    if (shape.vertices.size() > kMaxCount || shape.uvs.size() > kMaxCount ||
        shape.vertex_colours.size() > kMaxCount || shape.texture_names.size() > kMaxCount ||
        shape.primitives.size() > kMaxCount) {
        return Support::Unexpected(std::string("a shape table holds more than 65535 entries"));
    }
    for (const std::string& name : shape.texture_names) {
        if (name.find('\0') != std::string::npos)
            return Support::Unexpected(std::string("a texture name holds a NUL byte"));
    }
    for (const Primitive& primitive : shape.primitives) {
        if (primitive.indices.size() > kMaxCount)
            return Support::Unexpected(std::string("a primitive holds more than 65535 indices"));
    }
    return {};
}

struct Layout {
    std::size_t name_table = 0;
    std::vector<std::size_t> names;
    std::size_t vertices = 0;
    std::size_t uvs = 0;
    std::size_t colours = 0;
    std::size_t primitives = 0;
    std::vector<std::size_t> indices;
    std::size_t size = 0;
};

std::size_t Place(std::size_t& pos, std::size_t count, std::size_t element_size) {
    if (count == 0) return 0;
    const std::size_t at = pos;
    pos += count * element_size;
    return at;
}

Layout PlanLayout(const Shape& shape) {
    Layout layout;
    std::size_t pos = kHeaderSize + (shape.rect ? kRectSize : 0);
    layout.name_table = Place(pos, shape.texture_names.size(), kNameOffsetSize);
    for (const std::string& name : shape.texture_names) {
        layout.names.push_back(pos);
        pos += AlignUp(name.size() + 1);
    }
    layout.vertices = Place(pos, shape.vertices.size(), kPointSize);
    layout.uvs = Place(pos, shape.uvs.size(), kPointSize);
    layout.colours = Place(pos, shape.vertex_colours.size(), kColourSize);
    layout.primitives = Place(pos, shape.primitives.size(), kPrimitiveSize);
    for (const Primitive& primitive : shape.primitives) {
        layout.indices.push_back(primitive.indices.empty() ? 0 : pos);
        pos += AlignUp(primitive.indices.size() * kIndexSize);
    }
    layout.size = pos;
    return layout;
}

void WritePoints(Writer& out, std::size_t table,
                 const std::vector<std::array<uint32_t, 2>>& points) {
    for (std::size_t i = 0; i < points.size(); i++) {
        const std::size_t at = table + (i * kPointSize);
        out.U32(at, points[i][0]);
        out.U32(at + kFloatSize, points[i][1]);
    }
}

void WriteHeader(Writer& out, const Shape& shape, const Layout& layout) {
    out.U32(0, kMagic);
    out.U32(kVersionField, shape.unread_version);
    out.U32(kUnreadValueField, shape.unread_value);
    out.U32(kSizeField, static_cast<uint32_t>(layout.size));
    out.U32(kFlagsField, shape.flags | (shape.rect ? kFlagRect : 0U));
    out.U16(kVertexCountField, static_cast<uint16_t>(shape.vertices.size()));
    out.U16(kUvCountField, static_cast<uint16_t>(shape.uvs.size()));
    out.U16(kColourCountField, static_cast<uint16_t>(shape.vertex_colours.size()));
    out.U16(kNameCountField, static_cast<uint16_t>(shape.texture_names.size()));
    out.U16(kPrimitiveCountField, static_cast<uint16_t>(shape.primitives.size()));
    out.U16(kUnreadWordField, shape.unread_word);
    out.U32(kVertexTableField, static_cast<uint32_t>(layout.vertices));
    out.U32(kUvTableField, static_cast<uint32_t>(layout.uvs));
    out.U32(kColourTableField, static_cast<uint32_t>(layout.colours));
    out.U32(kNameTableField, static_cast<uint32_t>(layout.name_table));
    out.U32(kPrimitiveTableField, static_cast<uint32_t>(layout.primitives));
    if (shape.rect) {
        std::size_t at = kHeaderSize;
        for (const uint32_t value : *shape.rect) {
            out.U32(at, value);
            at += kFloatSize;
        }
    }
}

void WriteTables(Writer& out, const Shape& shape, const Layout& layout) {
    for (std::size_t i = 0; i < shape.texture_names.size(); i++) {
        out.U32(layout.name_table + (i * kNameOffsetSize), static_cast<uint32_t>(layout.names[i]));
        const std::string& name = shape.texture_names[i];
        const std::vector<uint8_t> bytes(name.begin(), name.end());
        out.Raw(layout.names[i], bytes);
    }
    WritePoints(out, layout.vertices, shape.vertices);
    WritePoints(out, layout.uvs, shape.uvs);
    for (std::size_t i = 0; i < shape.vertex_colours.size(); i++)
        out.Raw(layout.colours + (i * kColourSize), shape.vertex_colours[i]);
}

void WritePrimitives(Writer& out, const Shape& shape, const Layout& layout) {
    for (std::size_t i = 0; i < shape.primitives.size(); i++) {
        const Primitive& primitive = shape.primitives[i];
        const std::size_t record = layout.primitives + (i * kPrimitiveSize);
        const std::array<uint8_t, 4> head = {primitive.kind, primitive.draw_flags,
                                             primitive.texture, primitive.second_texture};
        out.Raw(record, head);
        out.U16(record + kPrimitiveIndexCountField,
                static_cast<uint16_t>(primitive.indices.size()));
        out.Raw(record + kPrimitiveUnreadBytesField, primitive.unread_bytes);
        out.Raw(record + kPrimitiveColourField, primitive.colour);
        out.U32(record + kPrimitiveIndexTableField, static_cast<uint32_t>(layout.indices[i]));
        for (std::size_t j = 0; j < primitive.indices.size(); j++)
            out.U16(layout.indices[i] + (j * kIndexSize), primitive.indices[j]);
    }
}

}

Support::Expected<ByteOrder, std::string> PackageByteOrder(std::span<const uint8_t> magic_file) {
    if (magic_file.size() != kBigEndianPackage.size())
        return Support::Unexpected(std::string("package magic is not 4 bytes"));
    return std::ranges::equal(magic_file, kBigEndianPackage) ? ByteOrder::Big : ByteOrder::Little;
}

Support::Expected<Shape, std::string> Read(std::span<const uint8_t> bytes, ByteOrder order) {
    const Reader r(bytes, order);
    if (!r.Has(0, kHeaderSize))
        return Support::Unexpected(std::string("shape header is truncated"));
    if (r.U32(0) != kMagic) {
        return Support::Unexpected(
            std::string("shape does not start with GE2D in this byte order"));
    }
    if (r.U32(kSizeField) != bytes.size()) {
        return Support::Unexpected(std::format("size field {} does not match the shape size {}",
                                               r.U32(kSizeField), bytes.size()));
    }
    const uint32_t flags = r.U32(kFlagsField);
    Shape shape{.unread_version = r.U32(kVersionField),
                .unread_value = r.U32(kUnreadValueField),
                .flags = flags & ~kFlagRect,
                .unread_word = r.U16(kUnreadWordField),
                .rect = std::nullopt,
                .vertices = {},
                .uvs = {},
                .vertex_colours = {},
                .texture_names = {},
                .primitives = {}};
    if ((flags & kFlagRect) != 0) {
        if (!r.Has(kHeaderSize, kRectSize))
            return Support::Unexpected(std::string("shape rect is truncated"));
        shape.rect = std::array<uint32_t, 4>{r.U32(kHeaderSize), r.U32(kHeaderSize + kFloatSize),
                                             r.U32(kHeaderSize + (2 * kFloatSize)),
                                             r.U32(kHeaderSize + (3 * kFloatSize))};
    }
    Claims claims;
    claims.Add(0, kHeaderSize + (shape.rect ? kRectSize : 0));
    auto vertices = ReadPoints(r, kVertexTable, claims);
    if (!vertices) return Support::Unexpected(vertices.error());
    shape.vertices = std::move(*vertices);
    auto uvs = ReadPoints(r, kUvTable, claims);
    if (!uvs) return Support::Unexpected(uvs.error());
    shape.uvs = std::move(*uvs);
    auto colours = ReadColours(r, claims);
    if (!colours) return Support::Unexpected(colours.error());
    shape.vertex_colours = std::move(*colours);
    auto names = ReadNames(r, claims);
    if (!names) return Support::Unexpected(names.error());
    shape.texture_names = std::move(*names);
    auto primitives = ReadPrimitives(r, claims);
    if (!primitives) return Support::Unexpected(primitives.error());
    shape.primitives = std::move(*primitives);
    auto claimed = claims.Check(bytes);
    if (!claimed) return Support::Unexpected(claimed.error());
    return shape;
}

Support::Expected<std::vector<uint8_t>, std::string> Write(const Shape& shape, ByteOrder order) {
    auto valid = Validate(shape);
    if (!valid) return Support::Unexpected(valid.error());
    const Layout layout = PlanLayout(shape);
    if (layout.size > kMaxFileSize)
        return Support::Unexpected(std::string("shape would exceed 4 GB"));
    Writer out(layout.size, order);
    WriteHeader(out, shape, layout);
    WriteTables(out, shape, layout);
    WritePrimitives(out, shape, layout);
    return std::move(out).Take();
}

}
