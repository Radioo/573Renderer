#include <catch2/catch_test_macros.hpp>

#include "formats/dxt_decode.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

struct Bgra {
    uint8_t b = 0;
    uint8_t g = 0;
    uint8_t r = 0;
    uint8_t a = 0;
    bool operator==(const Bgra&) const = default;
};

Bgra PixelAt(const std::vector<uint8_t>& buf, std::size_t pitch, std::size_t x, std::size_t y) {
    const std::size_t off = (y * pitch) + (x * 4);
    return {.b = buf[off], .g = buf[off + 1], .r = buf[off + 2], .a = buf[off + 3]};
}

}

TEST_CASE("EncodedSize follows block math") {
    CHECK(Dxt::EncodedSize(Dxt::kFmtDxt1, 4, 4) == 8);
    CHECK(Dxt::EncodedSize(Dxt::kFmtDxt5, 4, 4) == 16);
    CHECK(Dxt::EncodedSize(Dxt::kFmtDxt5, 8, 8) == 64);
    CHECK(Dxt::EncodedSize(Dxt::kFmtDxt1, 5, 5) == 32);
}

TEST_CASE("IsDxtFormat covers the afp id range") {
    CHECK_FALSE(Dxt::IsDxtFormat(0x10));
    CHECK(Dxt::IsDxtFormat(Dxt::kFmtDxt1));
    CHECK(Dxt::IsDxtFormat(Dxt::kFmtDxt5));
    CHECK_FALSE(Dxt::IsDxtFormat(0x1C));
}

TEST_CASE("DXT1 four-color block decodes solid endpoint color") {
    const std::array<uint8_t, 8> block = {0x00, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::vector<uint8_t> out(64, 0xAA);
    Dxt::Decompress(Dxt::kFmtDxt1, 4, 4, block, out, 16);
    for (std::size_t y = 0; y < 4; y++) {
        for (std::size_t x = 0; x < 4; x++) {
            CHECK(PixelAt(out, 16, x, y) == Bgra{0x00, 0x00, 0xFF, 0xFF});
        }
    }
}

TEST_CASE("DXT1 punchthrough mode makes index 3 transparent") {
    const std::array<uint8_t, 8> block = {0x00, 0x00, 0x00, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF};
    std::vector<uint8_t> out(64, 0xAA);
    Dxt::Decompress(Dxt::kFmtDxt1, 4, 4, block, out, 16);
    CHECK(PixelAt(out, 16, 0, 0) == Bgra{0x00, 0x00, 0x00, 0x00});
    CHECK(PixelAt(out, 16, 3, 3) == Bgra{0x00, 0x00, 0x00, 0x00});
}

TEST_CASE("DXT3 explicit alpha overwrites color alpha") {
    std::array<uint8_t, 16> block = {};
    block[0] = 0x0F;
    block[8] = 0x00;
    block[9] = 0xF8;
    std::vector<uint8_t> out(64, 0);
    Dxt::Decompress(Dxt::kFmtDxt3, 4, 4, block, out, 16);
    CHECK(PixelAt(out, 16, 0, 0) == Bgra{0x00, 0x00, 0xFF, 0xFF});
    CHECK(PixelAt(out, 16, 1, 0) == Bgra{0x00, 0x00, 0xFF, 0x00});
    CHECK(PixelAt(out, 16, 2, 0) == Bgra{0x00, 0x00, 0xFF, 0x00});
}

TEST_CASE("DXT5 interpolated alpha selects endpoints by index") {
    std::array<uint8_t, 16> block = {};
    block[0] = 0xFF;
    block[1] = 0x00;
    block[2] = 0x08;
    block[8] = 0x00;
    block[9] = 0xF8;
    std::vector<uint8_t> out(64, 0);
    Dxt::Decompress(Dxt::kFmtDxt5, 4, 4, block, out, 16);
    CHECK(PixelAt(out, 16, 0, 0).a == 0xFF);
    CHECK(PixelAt(out, 16, 1, 0).a == 0x00);
    CHECK(PixelAt(out, 16, 2, 0).a == 0xFF);
}

TEST_CASE("DXT5 color block is always four-color even when c0 <= c1") {
    std::array<uint8_t, 16> block = {};
    block[0] = 0xFF;
    block[1] = 0xFF;
    block[10] = 0xFF;
    block[11] = 0xFF;
    block[12] = 0xFF;
    block[13] = 0xFF;
    block[14] = 0xFF;
    block[15] = 0xFF;
    std::vector<uint8_t> out(64, 0);
    Dxt::Decompress(Dxt::kFmtDxt5, 4, 4, block, out, 16);
    CHECK(PixelAt(out, 16, 0, 0) == Bgra{0xAA, 0xAA, 0xAA, 0xFF});
    CHECK(PixelAt(out, 16, 3, 3) == Bgra{0xAA, 0xAA, 0xAA, 0xFF});
}

TEST_CASE("Decompress clips tail blocks to surface size") {
    const std::size_t pitch = 24;
    std::vector<uint8_t> out(6 * pitch, 0x77);
    std::vector<uint8_t> src(Dxt::EncodedSize(Dxt::kFmtDxt1, 6, 6));
    for (std::size_t i = 0; i < src.size(); i += 8) {
        src[i] = 0x00;
        src[i + 1] = 0xF8;
    }
    Dxt::Decompress(Dxt::kFmtDxt1, 6, 6, src, out, static_cast<int>(pitch));
    CHECK(PixelAt(out, pitch, 5, 5) == Bgra{0x00, 0x00, 0xFF, 0xFF});
    CHECK(PixelAt(out, pitch, 0, 0) == Bgra{0x00, 0x00, 0xFF, 0xFF});
}

TEST_CASE("Decompress rejects truncated source without writing") {
    const std::array<uint8_t, 4> short_src = {0x00, 0xF8, 0x00, 0x00};
    std::vector<uint8_t> out(64, 0x55);
    Dxt::Decompress(Dxt::kFmtDxt1, 4, 4, short_src, out, 16);
    CHECK(PixelAt(out, 16, 0, 0) == Bgra{0x55, 0x55, 0x55, 0x55});
}
