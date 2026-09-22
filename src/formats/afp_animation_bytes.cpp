#include "formats/afp_animation_detail.h"

#include "formats/afp_animation.h"
#include "formats/afp_byte_order.h"
#include "formats/afp_layout.h"
#include "formats/little_endian.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace AfpAnimation::Detail {

namespace {

constexpr uint32_t kMaxStringOffset = 1U << 18U;
constexpr uint32_t kReferenceOffsetMask = 0xFFFC;
constexpr unsigned kReferenceHighShift = 16;
constexpr uint32_t kReferenceHighMask = 3;

}

uint8_t ByteReader::U8(std::size_t off) {
    if (!Has(off, 1)) {
        failed_ = true;
        return 0;
    }
    return bytes_[off];
}

uint16_t ByteReader::U16(std::size_t off) {
    if (!Has(off, 2)) {
        failed_ = true;
        return 0;
    }
    return LittleEndian::ReadU16(bytes_, off);
}

uint32_t ByteReader::U32(std::size_t off) {
    if (!Has(off, 4)) {
        failed_ = true;
        return 0;
    }
    return LittleEndian::ReadU32(bytes_, off);
}

std::span<const uint8_t> ByteReader::Bytes(std::size_t off, std::size_t length) {
    if (!Has(off, length)) {
        failed_ = true;
        return {};
    }
    return bytes_.subspan(off, length);
}

bool ByteReader::ZeroFrom(std::size_t off) const {
    if (off > bytes_.size()) return false;
    return std::ranges::all_of(bytes_.subspan(off), [](uint8_t b) { return b == 0; });
}

Support::Expected<void, std::string> CheckTail(std::span<const uint8_t> record,
                                               std::size_t consumed) {
    const ByteReader r(record);
    if (consumed > record.size())
        return Support::Unexpected(std::string("tag data runs past its record"));
    if (record.size() - consumed >= AfpLayout::kAlignment || !r.ZeroFrom(consumed)) {
        return Support::Unexpected(
            std::format("{} unexplained bytes follow the tag data", record.size() - consumed));
    }
    return {};
}

Support::Expected<StringTable, std::string> StringTable::Read(std::span<const uint8_t> table,
                                                              std::vector<std::string>& strings) {
    if (!table.empty() && table[0] == AfpLayout::kScrambledStringsMarker)
        return Support::Unexpected(std::string("afp data is not restored"));
    StringTable result;
    std::size_t pos = 0;
    while (pos < table.size()) {
        const auto rest = table.subspan(pos);
        const auto nul = std::ranges::find(rest, uint8_t{0});
        if (nul == rest.end())
            return Support::Unexpected(std::format("string at {} has no terminator", pos));
        const auto length = static_cast<std::size_t>(nul - rest.begin());
        const std::size_t next = pos + AfpLayout::AlignUp(length + 1);
        if (next > table.size())
            return Support::Unexpected(std::format("string at {} pads past the table end", pos));
        if (!std::ranges::all_of(table.subspan(pos + length, next - pos - length),
                                 [](uint8_t b) { return b == 0; })) {
            return Support::Unexpected(std::format("string at {} has non-zero padding", pos));
        }
        strings.emplace_back(rest.begin(), nul);
        result.offsets_.push_back(static_cast<uint32_t>(pos));
        pos = next;
    }
    return result;
}

Support::Expected<StringId, std::string> StringTable::Resolve(uint16_t reference) const {
    const uint32_t off = (reference & kReferenceOffsetMask) |
                         ((reference & kReferenceHighMask) << kReferenceHighShift);
    const auto found = std::ranges::lower_bound(offsets_, off);
    if (found == offsets_.end() || *found != off) {
        return Support::Unexpected(
            std::format("string reference {:#x} is not the start of a string", reference));
    }
    return static_cast<StringId>(found - offsets_.begin());
}

std::vector<uint8_t> BuildStringTable(const std::vector<std::string>& strings,
                                      std::vector<uint32_t>& offsets) {
    std::vector<uint8_t> table;
    offsets.clear();
    for (const std::string& s : strings) {
        offsets.push_back(static_cast<uint32_t>(table.size()));
        table.insert(table.end(), s.begin(), s.end());
        table.push_back(0);
        table.resize(AfpLayout::AlignUp(table.size()), 0);
    }
    return table;
}

void ByteWriter::Record(uint8_t element_size, std::size_t off) {
    if (!swaps_.empty()) {
        AfpByteOrder::Swap& last = swaps_.back();
        if (last.element_size == element_size &&
            last.offset + (last.count * last.element_size) == off) {
            last.count++;
            return;
        }
    }
    swaps_.push_back(AfpByteOrder::Swap{.offset = off, .element_size = element_size, .count = 1});
}

void ByteWriter::U8(uint8_t value) {
    data_.push_back(value);
}

void ByteWriter::U16(uint16_t value) {
    Record(2, data_.size());
    LittleEndian::AppendU16(data_, value);
}

void ByteWriter::U32(uint32_t value) {
    Record(4, data_.size());
    LittleEndian::AppendU32(data_, value);
}

void ByteWriter::Raw(std::span<const uint8_t> bytes) {
    data_.insert(data_.end(), bytes.begin(), bytes.end());
}

void ByteWriter::OpaqueRaw(std::span<const uint8_t> bytes, const std::string& what) {
    Raw(bytes);
    if (!unknown_order_) unknown_order_ = what + " has no known byte order";
}

void ByteWriter::StringRef(StringId id) {
    if (id >= string_offsets_.size()) {
        Fail(std::format("string id {} is out of range", id));
        U16(0);
        return;
    }
    const uint32_t off = string_offsets_[id];
    if (off >= kMaxStringOffset) {
        Fail(std::format("string {} lies beyond the 18-bit reference range", id));
        U16(0);
        return;
    }
    U16(static_cast<uint16_t>((off & kReferenceOffsetMask) | (off >> kReferenceHighShift)));
}

void ByteWriter::Align4() {
    data_.resize(AfpLayout::AlignUp(data_.size()), 0);
}

void ByteWriter::PatchU16(std::size_t off, uint16_t value) {
    LittleEndian::WriteU16(data_, off, value);
}

void ByteWriter::PatchU32(std::size_t off, uint32_t value) {
    LittleEndian::WriteU32(data_, off, value);
}

void ByteWriter::Fail(std::string message) {
    if (!error_) error_ = std::move(message);
}

Native ByteWriter::Finish() && {
    Native native{.data = std::move(data_), .swaps = std::move(swaps_)};
    if (unknown_order_) native.swaps = Support::Unexpected(std::move(*unknown_order_));
    return native;
}

}
