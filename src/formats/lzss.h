#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Lzss {

constexpr size_t kRingBytes = 4096;

bool Decompress(std::span<const uint8_t> blob, std::vector<uint8_t>& out, std::string& err);

}
