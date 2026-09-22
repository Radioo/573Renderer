#include <catch2/catch_test_macros.hpp>

#include "binary_test_support.h"
#include "formats/avs_lz77.h"

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace {

std::string AsString(const std::vector<uint8_t>& v) {
    return {v.begin(), v.end()};
}

}

TEST_CASE("Decompress reads literals") {
    const std::vector<uint8_t> src = {0x07, 'A', 'B', 'C'};
    CHECK(AsString(AvsLz77::Decompress(src, 0)) == "ABC");
}

TEST_CASE("Decompress expands a window match") {
    const std::vector<uint8_t> src = {0x07, 'A', 'B', 'C', 0x00, 0x33, 0x00, 0x00};
    CHECK(AsString(AvsLz77::Decompress(src, 0)) == "ABCABCABC");
}

TEST_CASE("Decompress reads zero pre-history") {
    const std::vector<uint8_t> src = {0x00, 0x06, 0x41};
    const std::vector<uint8_t> expected = {0, 0, 0, 0};
    CHECK(AvsLz77::Decompress(src, 0) == expected);
}

TEST_CASE("Decompress stops at the expected size") {
    const std::vector<uint8_t> src = {0x1F, 'A', 'B', 'C', 'D', 'E'};
    CHECK(AsString(AvsLz77::Decompress(src, 3)) == "ABC");
}

TEST_CASE("Decompress stops at the zero-distance end marker") {
    const std::vector<uint8_t> src = {0x01, 'X', 0x00, 0x00, 'Y', 'Z'};
    CHECK(AsString(AvsLz77::Decompress(src, 0)) == "X");
}

namespace {

std::vector<uint8_t> SmallAlphabet(std::size_t size, uint32_t seed) {
    std::vector<uint8_t> out;
    uint32_t x = seed;
    for (std::size_t i = 0; i < size; i++) {
        x = ((x * 1103515245U) + 12345U) & 0x7FFFFFFFU;
        out.push_back(static_cast<uint8_t>((x >> 16U) % 6U));
    }
    return out;
}

std::vector<uint8_t> RandomBytes(std::size_t size, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> byte(0, 255);
    std::vector<uint8_t> out(size);
    for (auto& b : out)
        b = static_cast<uint8_t>(byte(rng));
    return out;
}

}

TEST_CASE("Compress ends a group of eight literals with an empty group") {
    const std::string text = "abcdefgh";
    const std::vector<uint8_t> src(text.begin(), text.end());
    CHECK(AvsLz77::Compress(src) == TestSupport::FromHex("ff6162636465666768000000"));
}

TEST_CASE("Compress matches into the zero pre-history") {
    const std::vector<uint8_t> src(5, 0);
    CHECK(AvsLz77::Compress(src) == TestSupport::FromHex("0001220000"));
}

TEST_CASE("Compress takes the longest match with overlap") {
    std::vector<uint8_t> src;
    for (int i = 0; i < 20; i++)
        src.insert(src.end(), {'a', 'b', 'c'});
    CHECK(AvsLz77::Compress(src) == TestSupport::FromHex("07616263003f003f003f00f00000"));
}

TEST_CASE("Compress breaks ties the way the binary tree does") {
    const std::vector<uint8_t> expected = TestSupport::FromHex(
        "ff0402020005050002ff0505050102020504f7010204011003000102f30103010100e001010004bb040300"
        "80040002029001fb0103031000050004036f000304020080020103726d0503a101050381030201d0fe0530"
        "0503050104020439000570061002040506c10681250307b0010120066101070004204e019103040300900"
        "0e003073118043008b008c003040880086000d008085004200b21040701019002d005a0000000");
    CHECK(AvsLz77::Compress(SmallAlphabet(200, 7)) == expected);
}

TEST_CASE("Compress of an empty buffer is an empty stream") {
    CHECK(AvsLz77::Compress({}) == TestSupport::FromHex("000000"));
    CHECK(AvsLz77::Decompress(AvsLz77::Compress({}), 0).empty());
}

TEST_CASE("Compress output decompresses to the input") {
    const std::vector<std::vector<uint8_t>> inputs = {
        RandomBytes(1, 1),        RandomBytes(4095, 2),           RandomBytes(4096, 3),
        RandomBytes(4097, 4),     RandomBytes(70000, 5),          std::vector<uint8_t>(9000, 0),
        SmallAlphabet(50000, 11), std::vector<uint8_t>(17, 0xAB),
    };
    for (const auto& input : inputs) {
        CHECK(AvsLz77::Decompress(AvsLz77::Compress(input), input.size()) == input);
    }
}
