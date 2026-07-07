#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace Bgra {

[[nodiscard]] std::vector<uint8_t> Crop(std::span<const uint8_t> atlas, int aw, int ah, int& x,
                                        int& y, int& w, int& h);

}
