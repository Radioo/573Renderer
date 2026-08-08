#include <catch2/catch_test_macros.hpp>

#include "formats/blowfish.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

std::string ToHex(const std::vector<uint8_t>& bytes) {
    static constexpr std::array<char, 16> kDigits = {'0', '1', '2', '3', '4', '5', '6', '7',
                                                     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const uint8_t b : bytes) {
        out.push_back(kDigits.at((size_t)b >> 4U));
        out.push_back(kDigits.at((size_t)b & 0x0FU));
    }
    return out;
}

}

TEST_CASE("blowfish matches the published single-block vectors", "[blowfish]") {
    const std::vector<uint8_t> zero_iv(Blowfish::kBlockBytes, 0);
    const std::vector<uint8_t> zero_block(Blowfish::kBlockBytes, 0);

    const std::vector<uint8_t> zero_key(8, 0);
    std::vector<uint8_t> out;
    Blowfish::Cipher(zero_key).EncryptCbc(zero_iv, zero_block, out);
    REQUIRE(ToHex(out) == "4ef997456198dd78");

    const std::vector<uint8_t> ones_key(8, 0xFF);
    const std::vector<uint8_t> ones_block(8, 0xFF);
    Blowfish::Cipher(ones_key).EncryptCbc(zero_iv, ones_block, out);
    REQUIRE(ToHex(out) == "51866fd5b85ecb8a");
}

TEST_CASE("blowfish cbc round-trips with a non-zero iv", "[blowfish]") {
    const std::vector<uint8_t> key = {'o', 'm', 'E', '8', '7', '2', 'M', '2'};
    const std::vector<uint8_t> iv = {'t', 'u', 'k', 'u', 'p', 'y', 'p', 'y'};
    std::vector<uint8_t> plain(64, 0);
    for (size_t i = 0; i < plain.size(); i++)
        plain[i] = (uint8_t)(i * 7);

    const Blowfish::Cipher cipher(key);
    std::vector<uint8_t> encoded;
    cipher.EncryptCbc(iv, plain, encoded);
    REQUIRE(encoded.size() == plain.size());
    REQUIRE(encoded != plain);

    std::vector<uint8_t> decoded;
    cipher.DecryptCbc(iv, encoded, decoded);
    REQUIRE(decoded == plain);
}

TEST_CASE("blowfish cbc ignores a trailing partial block", "[blowfish]") {
    const std::vector<uint8_t> key = {'k', 'e', 'y'};
    const std::vector<uint8_t> iv(Blowfish::kBlockBytes, 0);
    const std::vector<uint8_t> plain(13, 0x5A);

    std::vector<uint8_t> encoded;
    Blowfish::Cipher(key).EncryptCbc(iv, plain, encoded);
    REQUIRE(encoded.size() == 8);
}
