#pragma once

#include "formats/binary_xml.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Ifs::Detail {

constexpr uint32_t kSignature = 0x6CAD8F89U;
constexpr uint16_t kFlagComplement = 0xFFFF;
constexpr std::size_t kHeaderSize = 20;
constexpr std::size_t kMd5Size = 16;
constexpr const char* kRootName = "imgfs";
constexpr const char* kInfoName = "_info_";

struct Layout {
    std::vector<uint32_t> offsets;
    uint32_t data_size = 0;
};

[[nodiscard]] Layout PackLargestFirst(std::span<const uint32_t> sizes);

[[nodiscard]] uint32_t TreeSize(const BinaryXml::Document& manifest);

[[nodiscard]] constexpr uint32_t AlignTo(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1U) & ~(alignment - 1U);
}

}
