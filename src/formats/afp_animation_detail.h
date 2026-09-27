#pragma once

#include "formats/afp_animation.h"
#include "formats/afp_byte_order.h"
#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace AfpAnimation::Detail {

constexpr std::size_t kMaxNesting = 256;

class ByteReader {
public:
    explicit ByteReader(std::span<const uint8_t> bytes) : bytes_(bytes) {}

    [[nodiscard]] bool Failed() const { return failed_; }
    [[nodiscard]] bool Has(std::size_t off, std::size_t length) const {
        return off <= bytes_.size() && length <= bytes_.size() - off;
    }

    [[nodiscard]] uint8_t U8(std::size_t off);
    [[nodiscard]] uint16_t U16(std::size_t off);
    [[nodiscard]] int16_t S16(std::size_t off) { return static_cast<int16_t>(U16(off)); }
    [[nodiscard]] uint32_t U32(std::size_t off);
    [[nodiscard]] int32_t S32(std::size_t off) { return static_cast<int32_t>(U32(off)); }
    [[nodiscard]] std::span<const uint8_t> Bytes(std::size_t off, std::size_t length);
    [[nodiscard]] bool ZeroFrom(std::size_t off) const;

private:
    std::span<const uint8_t> bytes_;
    bool failed_ = false;
};

class StringTable {
public:
    [[nodiscard]] static Support::Expected<StringTable, std::string>
    Read(std::span<const uint8_t> table, std::vector<std::string>& strings);

    [[nodiscard]] Support::Expected<StringId, std::string> Resolve(uint16_t reference) const;

private:
    std::vector<uint32_t> offsets_;
};

class ByteWriter {
public:
    explicit ByteWriter(std::vector<uint32_t> string_offsets)
        : string_offsets_(std::move(string_offsets)) {}

    [[nodiscard]] std::size_t Position() const { return data_.size(); }
    [[nodiscard]] bool Failed() const { return error_.has_value(); }
    [[nodiscard]] const std::string& Error() const { return *error_; }

    void U8(uint8_t value);
    void U16(uint16_t value);
    void S16(int16_t value) { U16(static_cast<uint16_t>(value)); }
    void U32(uint32_t value);
    void S32(int32_t value) { U32(static_cast<uint32_t>(value)); }
    void Raw(std::span<const uint8_t> bytes);
    void OpaqueRaw(std::span<const uint8_t> bytes, const std::string& what);
    void StringRef(StringId id);
    void Align4();
    void PatchU16(std::size_t off, uint16_t value);
    void PatchU32(std::size_t off, uint32_t value);
    void Fail(std::string message);

    [[nodiscard]] Native Finish() &&;

private:
    void Record(uint8_t element_size, std::size_t off);

    std::vector<uint8_t> data_;
    std::vector<AfpByteOrder::Swap> swaps_;
    std::vector<uint32_t> string_offsets_;
    std::optional<std::string> unknown_order_;
    std::optional<std::string> error_;
};

[[nodiscard]] Support::Expected<void, std::string> CheckTail(std::span<const uint8_t> record,
                                                             std::size_t consumed);

[[nodiscard]] Filter ReadFilter(std::span<const uint8_t> bytes);

[[nodiscard]] std::vector<uint8_t> BuildStringTable(const std::vector<std::string>& strings,
                                                    std::vector<uint32_t>& offsets);

[[nodiscard]] Support::Expected<Bytecode, std::string> ReadBytecode(std::span<const uint8_t> bytes,
                                                                    const StringTable& strings);

void WriteBytecode(ByteWriter& out, const Bytecode& bytecode);

[[nodiscard]] Support::Expected<Placement, std::string>
ReadPlacement(std::span<const uint8_t> record, const StringTable& strings);

void WritePlacement(ByteWriter& out, const Placement& placement);

}
