#include <catch2/catch_test_macros.hpp>

#include "formats/afp_byte_order.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace {

using AfpByteOrder::Swap;

std::vector<uint8_t> Words(std::initializer_list<uint16_t> words) {
    std::vector<uint8_t> out;
    for (const uint16_t w : words) {
        out.push_back(static_cast<uint8_t>(w & 0xFFU));
        out.push_back(static_cast<uint8_t>(w >> 8U));
    }
    return out;
}

void PutU32(std::vector<uint8_t>& data, std::size_t off, uint32_t value) {
    for (std::size_t i = 0; i < 4; i++) {
        data[off + i] = static_cast<uint8_t>((value >> (8U * i)) & 0xFFU);
    }
}

std::vector<uint8_t> NativeAnimation() {
    std::vector<uint8_t> data(64, 0);
    data[0] = 0x08;
    data[1] = 0xB2;
    data[2] = 0xD0;
    data[3] = 0xC1;
    PutU32(data, 4, 64);
    data[8] = 0x00;
    data[9] = 0x02;
    PutU32(data, 48, 56);
    PutU32(data, 52, 8);
    data[60] = 'a';
    data[61] = 'b';
    return data;
}

const std::vector<Swap> kHeaderSwaps = {
    Swap{.offset = 0, .element_size = 4, .count = 2},
    Swap{.offset = 8, .element_size = 2, .count = 1},
    Swap{.offset = 48, .element_size = 4, .count = 2},
};

}

TEST_CASE("ReadScript decodes skips, swap runs and skip blocks") {
    const auto swaps = AfpByteOrder::ReadScript(Words({0x4080, 0x2002, 0x0096, 0x4000, 0x0000}));
    REQUIRE(swaps.has_value());
    CHECK(*swaps == std::vector<Swap>{
                        Swap{.offset = 0, .element_size = 4, .count = 2},
                        Swap{.offset = 12, .element_size = 2, .count = 1},
                        Swap{.offset = 314, .element_size = 4, .count = 1},
                    });
}

TEST_CASE("ReadScript rejects malformed scripts") {
    CHECK_FALSE(AfpByteOrder::ReadScript(Words({0x8000, 0x0000})).has_value());
    CHECK_FALSE(AfpByteOrder::ReadScript(Words({0x4080})).has_value());
    CHECK_FALSE(AfpByteOrder::ReadScript(std::vector<uint8_t>{0x80, 0x40, 0x00}).has_value());
}

TEST_CASE("WriteScript merges adjacent elements of one size into a run") {
    const std::vector<Swap> swaps = {
        Swap{.offset = 0, .element_size = 4, .count = 1},
        Swap{.offset = 4, .element_size = 4, .count = 1},
        Swap{.offset = 8, .element_size = 2, .count = 1},
    };
    const auto script = AfpByteOrder::WriteScript(swaps);
    REQUIRE(script.has_value());
    CHECK(*script == Words({0x4080, 0x2000, 0x0000}));
}

TEST_CASE("WriteScript splits runs at 64 elements") {
    const std::vector<Swap> swaps = {Swap{.offset = 0, .element_size = 2, .count = 65}};
    const auto script = AfpByteOrder::WriteScript(swaps);
    REQUIRE(script.has_value());
    CHECK(*script == Words({0x3F80, 0x2000, 0x0000}));
}

TEST_CASE("WriteScript puts short gaps in the swap word and long gaps in a skip word") {
    const auto short_gap = AfpByteOrder::WriteScript(
        std::vector<Swap>{Swap{.offset = 254, .element_size = 4, .count = 1}});
    REQUIRE(short_gap.has_value());
    CHECK(*short_gap == Words({0x407F, 0x0000}));

    const auto long_gap = AfpByteOrder::WriteScript(
        std::vector<Swap>{Swap{.offset = 300, .element_size = 4, .count = 1}});
    REQUIRE(long_gap.has_value());
    CHECK(*long_gap == Words({0x0096, 0x4000, 0x0000}));

    const auto whole_blocks = AfpByteOrder::WriteScript(
        std::vector<Swap>{Swap{.offset = 512, .element_size = 8, .count = 1}});
    REQUIRE(whole_blocks.has_value());
    CHECK(*whole_blocks == Words({0x0100, 0x6000, 0x0000}));
}

TEST_CASE("WriteScript output reads back as the same elements") {
    const std::vector<Swap> swaps = {
        Swap{.offset = 6, .element_size = 2, .count = 3},
        Swap{.offset = 1000, .element_size = 4, .count = 70},
        Swap{.offset = 20000, .element_size = 8, .count = 2},
    };
    const auto script = AfpByteOrder::WriteScript(swaps);
    REQUIRE(script.has_value());
    const auto back = AfpByteOrder::ReadScript(*script);
    REQUIRE(back.has_value());
    const auto again = AfpByteOrder::WriteScript(*back);
    REQUIRE(again.has_value());
    CHECK(*again == *script);
    std::size_t elements = 0;
    for (const Swap& s : *back)
        elements += s.count;
    CHECK(elements == 75);
}

TEST_CASE("WriteScript rejects swaps it cannot express") {
    CHECK_FALSE(AfpByteOrder::WriteScript(
                    std::vector<Swap>{Swap{.offset = 3, .element_size = 2, .count = 1}})
                    .has_value());
    CHECK_FALSE(AfpByteOrder::WriteScript(
                    std::vector<Swap>{Swap{.offset = 0, .element_size = 3, .count = 1}})
                    .has_value());
    CHECK_FALSE(AfpByteOrder::WriteScript(
                    std::vector<Swap>{Swap{.offset = 0, .element_size = 4, .count = 0}})
                    .has_value());
    CHECK_FALSE(AfpByteOrder::WriteScript(std::vector<Swap>{
                                              Swap{.offset = 0, .element_size = 4, .count = 2},
                                              Swap{.offset = 4, .element_size = 4, .count = 1},
                                          })
                    .has_value());
}

TEST_CASE("Store swaps and scrambles, Restore undoes both") {
    const std::vector<uint8_t> native = NativeAnimation();
    const auto stored = AfpByteOrder::Store(native, kHeaderSwaps, true);
    REQUIRE(stored.has_value());
    CHECK((*stored)[0] == 0xC1);
    CHECK((*stored)[3] == 0x08);
    CHECK((*stored)[56] == 0x80);
    CHECK((*stored)[60] == static_cast<uint8_t>('a' + 128 + 4));

    const auto script = AfpByteOrder::WriteScript(kHeaderSwaps);
    REQUIRE(script.has_value());
    const auto restored = AfpByteOrder::Restore(*stored, *script);
    REQUIRE(restored.has_value());
    CHECK(restored->data == native);
    CHECK(restored->strings_scrambled);
    CHECK(restored->swaps == kHeaderSwaps);
}

TEST_CASE("Restore leaves native data and plain strings alone") {
    const std::vector<uint8_t> native = NativeAnimation();
    const auto script = AfpByteOrder::WriteScript(kHeaderSwaps);
    REQUIRE(script.has_value());
    const auto restored = AfpByteOrder::Restore(native, *script);
    REQUIRE(restored.has_value());
    CHECK(restored->data == native);
    CHECK_FALSE(restored->strings_scrambled);

    const auto plain = AfpByteOrder::Store(native, kHeaderSwaps, false);
    REQUIRE(plain.has_value());
    CHECK((*plain)[56] == 0x00);
}

TEST_CASE("Restore rejects data that is not an animation or has an unusual string table") {
    std::vector<uint8_t> not_afp = NativeAnimation();
    not_afp[1] = 'X';
    const auto script = AfpByteOrder::WriteScript(kHeaderSwaps);
    REQUIRE(script.has_value());
    CHECK_FALSE(AfpByteOrder::Restore(not_afp, *script).has_value());

    std::vector<uint8_t> unusual = NativeAnimation();
    unusual[56] = 0x41;
    CHECK_FALSE(AfpByteOrder::Restore(unusual, *script).has_value());

    const std::vector<uint8_t> tiny = {0x08, 0xB2};
    CHECK_FALSE(AfpByteOrder::Restore(tiny, *script).has_value());
}

TEST_CASE("Store rejects swaps outside the data") {
    const std::vector<uint8_t> native = NativeAnimation();
    const std::vector<Swap> outside = {Swap{.offset = 62, .element_size = 4, .count = 1}};
    CHECK_FALSE(AfpByteOrder::Store(native, outside, false).has_value());
}

TEST_CASE("Restore treats old PAF data as native and leaves its bytes alone") {
    const auto script = AfpByteOrder::WriteScript(kHeaderSwaps);
    REQUIRE(script.has_value());
    for (const std::vector<uint8_t>& magic :
         {std::vector<uint8_t>{0x50, 0x46, 0x41}, std::vector<uint8_t>{0xD0, 0xC6, 0xC1}}) {
        std::vector<uint8_t> paf = NativeAnimation();
        std::ranges::copy(magic, paf.begin());
        paf[56] = 0x80;
        const auto restored = AfpByteOrder::Restore(paf, *script);
        REQUIRE(restored.has_value());
        CHECK(restored->data == paf);
        CHECK_FALSE(restored->strings_scrambled);
    }
    std::vector<uint8_t> mixed = NativeAnimation();
    mixed[0] = 0x50;
    mixed[1] = 0xC6;
    mixed[2] = 0x41;
    CHECK_FALSE(AfpByteOrder::Restore(mixed, *script).has_value());
}

TEST_CASE("Restore leaves the string table of data version 1 scrambled") {
    std::vector<uint8_t> version_one = NativeAnimation();
    version_one[8] = 0x01;
    version_one[9] = 0x00;
    version_one[56] = 0x80;
    const auto script = AfpByteOrder::WriteScript(kHeaderSwaps);
    REQUIRE(script.has_value());
    const auto restored = AfpByteOrder::Restore(version_one, *script);
    REQUIRE(restored.has_value());
    CHECK(restored->data == version_one);
    CHECK_FALSE(restored->strings_scrambled);
}

TEST_CASE("Restore ignores the script when the data is already native") {
    const std::vector<uint8_t> native = NativeAnimation();
    const auto restored = AfpByteOrder::Restore(native, std::vector<uint8_t>{0x80});
    REQUIRE(restored.has_value());
    CHECK(restored->data == native);
    CHECK(restored->swaps.empty());
}

TEST_CASE("Store refuses to scramble the string table of data version 1") {
    std::vector<uint8_t> version_one = NativeAnimation();
    version_one[8] = 0x01;
    version_one[9] = 0x00;
    CHECK_FALSE(AfpByteOrder::Store(version_one, kHeaderSwaps, true).has_value());
    CHECK(AfpByteOrder::Store(version_one, kHeaderSwaps, false).has_value());
}
