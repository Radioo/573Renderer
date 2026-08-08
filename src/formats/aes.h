#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Aes {

constexpr size_t kKeyBytes = 32;
constexpr size_t kBlockBytes = 16;

bool DecryptCbcCts(std::span<const uint8_t> key, std::span<const uint8_t> iv,
                   std::span<const uint8_t> cipher, std::vector<uint8_t>& out, std::string& err);

}
