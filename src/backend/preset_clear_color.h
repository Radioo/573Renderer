#pragma once

#include <cstdint>
#include <optional>

namespace Backend {

struct ClearInputs {
    bool preset_active = false;
    bool exporting = false;
    bool bg_transparent = false;
    std::uint32_t color = 0;
};

std::optional<std::uint32_t> FrameClearColor(const ClearInputs& inputs);

}
