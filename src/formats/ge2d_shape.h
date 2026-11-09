#pragma once

#include "support/expected.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Ge2dShape {

enum class ByteOrder : uint8_t { Big, Little };

struct Primitive {
    uint8_t kind = 0;
    uint8_t draw_flags = 0;
    uint8_t texture = 0;
    uint8_t second_texture = 0;
    std::array<uint8_t, 2> unread_bytes{};
    std::array<uint8_t, 4> colour{};
    std::vector<uint16_t> indices;

    friend bool operator==(const Primitive&, const Primitive&) = default;
};

struct Shape {
    uint32_t unread_version = 0;
    uint32_t unread_value = 0;
    uint32_t flags = 0;
    uint16_t unread_word = 0;
    std::optional<std::array<uint32_t, 4>> rect;
    std::vector<std::array<uint32_t, 2>> vertices;
    std::vector<std::array<uint32_t, 2>> uvs;
    std::vector<std::array<uint8_t, 4>> vertex_colours;
    std::vector<std::string> texture_names;
    std::vector<Primitive> primitives;

    friend bool operator==(const Shape&, const Shape&) = default;
};

[[nodiscard]] Support::Expected<ByteOrder, std::string>
PackageByteOrder(std::span<const uint8_t> magic_file);

[[nodiscard]] Support::Expected<Shape, std::string> Read(std::span<const uint8_t> bytes,
                                                         ByteOrder order);

[[nodiscard]] Support::Expected<std::vector<uint8_t>, std::string> Write(const Shape& shape,
                                                                         ByteOrder order);

}
