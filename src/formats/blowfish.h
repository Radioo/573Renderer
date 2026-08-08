#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Blowfish {

constexpr size_t kBlockBytes = 8;
constexpr size_t kRounds = 16;
constexpr size_t kBoxEntries = 256;

class Cipher {
public:
    explicit Cipher(std::span<const uint8_t> key);

    void EncryptCbc(std::span<const uint8_t> iv, std::span<const uint8_t> in,
                    std::vector<uint8_t>& out) const;

    void DecryptCbc(std::span<const uint8_t> iv, std::span<const uint8_t> in,
                    std::vector<uint8_t>& out) const;

private:
    void EncryptBlock(uint32_t& left, uint32_t& right) const;
    void DecryptBlock(uint32_t& left, uint32_t& right) const;
    [[nodiscard]] uint32_t Feistel(uint32_t x) const;

    std::array<uint32_t, kRounds + 2> p_{};
    std::array<uint32_t, 4 * kBoxEntries> s_{};
};

}
