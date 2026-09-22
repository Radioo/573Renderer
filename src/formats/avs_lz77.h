#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace AvsLz77 {

[[nodiscard]] std::vector<uint8_t> Decompress(std::span<const uint8_t> src,
                                              std::size_t expected_size);

[[nodiscard]] std::vector<uint8_t> Compress(std::span<const uint8_t> src);

}
