#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace BigEndian {

[[nodiscard]] inline uint16_t ReadU16(std::span<const uint8_t> bytes, std::size_t off) {
    return static_cast<uint16_t>((static_cast<unsigned>(bytes[off]) << 8U) | bytes[off + 1]);
}

[[nodiscard]] inline uint32_t ReadU32(std::span<const uint8_t> bytes, std::size_t off) {
    return (static_cast<uint32_t>(bytes[off]) << 24U) |
           (static_cast<uint32_t>(bytes[off + 1]) << 16U) |
           (static_cast<uint32_t>(bytes[off + 2]) << 8U) | static_cast<uint32_t>(bytes[off + 3]);
}

inline void AppendU16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<uint8_t>(value & 0xFFU));
}

inline void WriteU16(std::span<uint8_t> bytes, std::size_t off, uint16_t value) {
    bytes[off] = static_cast<uint8_t>(value >> 8U);
    bytes[off + 1] = static_cast<uint8_t>(value & 0xFFU);
}

inline void WriteU32(std::span<uint8_t> bytes, std::size_t off, uint32_t value) {
    for (std::size_t i = 0; i < 4; i++)
        bytes[off + i] = static_cast<uint8_t>((value >> (24U - (8U * i))) & 0xFFU);
}

inline void AppendU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24U) & 0xFFU));
    out.push_back(static_cast<uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<uint8_t>(value & 0xFFU));
}

}
