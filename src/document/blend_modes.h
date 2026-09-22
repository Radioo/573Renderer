#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace Document {

struct BlendMode {
    uint8_t value = 0;
    std::string_view name;
};

[[nodiscard]] std::span<const BlendMode> BlendModes();

[[nodiscard]] std::string BlendName(uint8_t value);

}
