#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace Document {

enum class PropertyGroup : uint8_t { None, Matrix, Colour };

[[nodiscard]] PropertyGroup GroupOf(std::string_view property, bool three_d);

[[nodiscard]] uint32_t GroupBit(PropertyGroup group);

[[nodiscard]] std::optional<std::vector<int64_t>> IdentityOf(std::string_view property);

}
