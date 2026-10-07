#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace LittleEndian {

[[nodiscard]] inline uint16_t ReadU16(std::span<const uint8_t> bytes, std::size_t off) {
    return static_cast<uint16_t>(bytes[off] | (static_cast<unsigned>(bytes[off + 1]) << 8U));
}

[[nodiscard]] inline uint32_t ReadU32(std::span<const uint8_t> bytes, std::size_t off) {
    return static_cast<uint32_t>(bytes[off]) | (static_cast<uint32_t>(bytes[off + 1]) << 8U) |
           (static_cast<uint32_t>(bytes[off + 2]) << 16U) |
           (static_cast<uint32_t>(bytes[off + 3]) << 24U);
}

inline void AppendU16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFFU));
    out.push_back(static_cast<uint8_t>(value >> 8U));
}

inline void AppendU32(std::vector<uint8_t>& out, uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<uint8_t>((value >> shift) & 0xFFU));
}

inline void WriteU16(std::span<uint8_t> bytes, std::size_t off, uint16_t value) {
    bytes[off] = static_cast<uint8_t>(value & 0xFFU);
    bytes[off + 1] = static_cast<uint8_t>(value >> 8U);
}

inline void WriteU32(std::span<uint8_t> bytes, std::size_t off, uint32_t value) {
    for (std::size_t i = 0; i < 4; i++)
        bytes[off + i] = static_cast<uint8_t>((value >> (8U * i)) & 0xFFU);
}

}
