#pragma once

#include <cstdint>

namespace Hsl {

[[nodiscard]] uint32_t AdjustArgb(uint32_t argb, float dh, float ds, float dl);

}
