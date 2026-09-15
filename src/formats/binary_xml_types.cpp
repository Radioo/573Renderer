#include "formats/binary_xml_types.h"

#include "formats/binary_xml.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace BinaryXml::Detail {

namespace {

constexpr uint8_t kTypeMask = 0x3F;
constexpr uint8_t kHighBit = 0x80;
constexpr uint8_t kMaxType = 56;

constexpr std::array<uint8_t, kMaxType + 1> kFixedSizes = {
    0, 0, 1,  1,  2,  2,  4,  4,  8, 8, 0,  0,  4,  4,  4,  8,  2,  2, 4,
    4, 8, 8,  16, 16, 8,  16, 3,  3, 6, 6,  12, 12, 24, 24, 12, 24, 4, 4,
    8, 8, 16, 16, 32, 32, 16, 32, 0, 0, 16, 16, 16, 16, 1,  2,  3,  4, 16,
};

}

bool IsValidType(uint8_t type) {
    const auto base = static_cast<uint8_t>(type & kTypeMask);
    return (type & kHighBit) == 0 && base >= Type::kVoid && base <= kMaxType;
}

ValueClass ClassOf(uint8_t type) {
    if ((type & kArrayFlag) != 0) return ValueClass::Prefixed;
    switch (type) {
    case Type::kVoid:
    case Type::kArrayMarker:
        return ValueClass::None;
    case Type::kS8:
    case Type::kU8:
    case Type::kBool:
        return ValueClass::Byte;
    case Type::kS16:
    case Type::kU16:
    case Type::k2S8:
    case Type::k2U8:
    case Type::k2B:
        return ValueClass::Word;
    case Type::kBin:
    case Type::kStr:
    case Type::kAttribute:
        return ValueClass::Prefixed;
    default:
        return ValueClass::Fixed;
    }
}

std::size_t FixedSize(uint8_t type) {
    return kFixedSizes.at(static_cast<std::size_t>(type & kTypeMask));
}

}
