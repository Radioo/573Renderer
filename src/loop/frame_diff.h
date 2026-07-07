#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace Loop {

[[nodiscard]] double FrameDiffMad(std::span<const uint8_t> a, std::span<const uint8_t> b);

}
