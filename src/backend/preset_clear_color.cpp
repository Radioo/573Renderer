#include "backend/preset_clear_color.h"

#include <cstdint>
#include <optional>

namespace Backend {

std::optional<std::uint32_t> FrameClearColor(const ClearInputs& inputs) {
    if (inputs.exporting && inputs.bg_transparent) return std::nullopt;
    if (!inputs.preset_active) return 0U;
    return inputs.color;
}

}
