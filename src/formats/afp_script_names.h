#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace AfpScript {

[[nodiscard]] std::optional<std::string_view> BuiltinName(uint16_t id);

[[nodiscard]] std::optional<uint16_t> BuiltinNamed(std::string_view name);

[[nodiscard]] std::span<const std::string_view> BuiltinNames();

}
