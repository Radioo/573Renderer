#include <catch2/catch_test_macros.hpp>

#include "formats/aes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> FromHex(const std::string& hex) {
    std::vector<uint8_t> out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2)
        out.push_back((uint8_t)std::stoul(hex.substr(i, 2), nullptr, 16));
    return out;
}

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

TEST_CASE("AES-256-CBC matches the NIST SP 800-38A vectors", "[aes]") {
    const std::vector<uint8_t> key =
        FromHex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
    const std::vector<uint8_t> iv = FromHex("000102030405060708090a0b0c0d0e0f");
    const std::vector<uint8_t> cipher = FromHex("f58c4c04d6e5f1ba779eabfb5f7bfbd6"
                                                "9cfc4e967edb808d679f777bc6702c7d"
                                                "39f23369a9d9bacfa530e26304231461"
                                                "b2eb05e2c39be9fcda6c19078c6a9d1b");

    std::vector<uint8_t> plain;
    std::string err;
    REQUIRE(Aes::DecryptCbcCts(key, iv, cipher, plain, err));
    REQUIRE(ToHex(plain) == "6bc1bee22e409f96e93d7e117393172a"
                            "ae2d8a571e03ac9c9eb76fac45af8e51"
                            "30c81c46a35ce411e5fbc1191a0a52ef"
                            "f69f2445df4f9b17ad2b417be66c3710");
}

TEST_CASE("AES-256 CBC-CTS keeps a partial trailing block", "[aes]") {
    const std::vector<uint8_t> key =
        FromHex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
    const std::vector<uint8_t> iv = FromHex("000102030405060708090a0b0c0d0e0f");
    const std::vector<uint8_t> cipher = FromHex("f58c4c04d6e5f1ba779eabfb5f7bfbd6"
                                                "9cfc4e967edb808d679f777bc6702c7d"
                                                "39f233");

    std::vector<uint8_t> plain;
    std::string err;
    REQUIRE(Aes::DecryptCbcCts(key, iv, cipher, plain, err));
    REQUIRE(plain.size() == cipher.size());
}
