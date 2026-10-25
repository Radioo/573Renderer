#pragma once

#include <cstddef>
#include <cstdint>

namespace BinaryXml::Detail {

constexpr uint8_t kMagic = 0xA0;
constexpr uint8_t kNodeEnd = 0xFE;
constexpr uint8_t kDocumentEnd = 0xFF;
constexpr uint8_t kEncodingComplement = 0xFF;
constexpr std::size_t kHeaderSize = 8;
constexpr std::size_t kLengthSize = 4;

enum class ValueClass : uint8_t { None, Byte, Word, Prefixed, Fixed };

[[nodiscard]] bool IsValidType(uint8_t type);

[[nodiscard]] ValueClass ClassOf(uint8_t type);

[[nodiscard]] std::size_t FixedSize(uint8_t type);

[[nodiscard]] constexpr std::size_t PaddedTo4(std::size_t n) {
    return (n + 3U) & ~std::size_t{3};
}

}
