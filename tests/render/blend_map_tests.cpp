#include <catch2/catch_test_macros.hpp>

#include "render/blend_map.h"

#include <cstdint>

namespace {

bool RowIs(uint32_t mode, uint32_t op, uint32_t src, uint32_t dst) {
    const Blend::D3d9Blend b = Blend::MapAfpMode(mode);
    return b.op == op && b.src == src && b.dst == dst;
}

}

TEST_CASE("op and factor constants carry the frozen D3D9 enum values") {
    CHECK(Blend::kOpAdd == 1U);
    CHECK(Blend::kOpRevSubtract == 3U);
    CHECK(Blend::kOpMin == 4U);
    CHECK(Blend::kOpMax == 5U);
    CHECK(Blend::kFactorZero == 1U);
    CHECK(Blend::kFactorOne == 2U);
    CHECK(Blend::kFactorSrcColor == 3U);
    CHECK(Blend::kFactorSrcAlpha == 5U);
    CHECK(Blend::kFactorInvSrcAlpha == 6U);
}

TEST_CASE("mode 0 and unknown modes fall back to normal alpha") {
    CHECK(RowIs(0, Blend::kOpAdd, Blend::kFactorSrcAlpha, Blend::kFactorInvSrcAlpha));
    CHECK(RowIs(2, Blend::kOpAdd, Blend::kFactorSrcAlpha, Blend::kFactorInvSrcAlpha));
    CHECK(RowIs(7, Blend::kOpAdd, Blend::kFactorSrcAlpha, Blend::kFactorInvSrcAlpha));
    CHECK(RowIs(12345, Blend::kOpAdd, Blend::kFactorSrcAlpha, Blend::kFactorInvSrcAlpha));
}

TEST_CASE("the additive trio maps to srcalpha plus one") {
    CHECK(RowIs(4, Blend::kOpAdd, Blend::kFactorSrcAlpha, Blend::kFactorOne));
    CHECK(RowIs(8, Blend::kOpAdd, Blend::kFactorSrcAlpha, Blend::kFactorOne));
    CHECK(RowIs(0x4F, Blend::kOpAdd, Blend::kFactorSrcAlpha, Blend::kFactorOne));
    CHECK(Blend::IsAdditiveAfpMode(4));
    CHECK(Blend::IsAdditiveAfpMode(8));
    CHECK(Blend::IsAdditiveAfpMode(0x4F));
    CHECK_FALSE(Blend::IsAdditiveAfpMode(0));
    CHECK_FALSE(Blend::IsAdditiveAfpMode(3));
    CHECK_FALSE(Blend::IsAdditiveAfpMode(5));
}

TEST_CASE("lighten darken multiply and subtractive rows") {
    CHECK(RowIs(5, Blend::kOpMax, Blend::kFactorSrcAlpha, Blend::kFactorOne));
    CHECK(RowIs(6, Blend::kOpMin, Blend::kFactorSrcAlpha, Blend::kFactorOne));
    CHECK(RowIs(3, Blend::kOpAdd, Blend::kFactorZero, Blend::kFactorSrcColor));
    CHECK(RowIs(9, Blend::kOpRevSubtract, Blend::kFactorSrcAlpha, Blend::kFactorOne));
    CHECK(RowIs(0x46, Blend::kOpRevSubtract, Blend::kFactorSrcAlpha, Blend::kFactorOne));
}

TEST_CASE("alpha coverage policy is max of one and one") {
    CHECK(Blend::kAlphaCoverage.op == Blend::kOpMax);
    CHECK(Blend::kAlphaCoverage.src == Blend::kFactorOne);
    CHECK(Blend::kAlphaCoverage.dst == Blend::kFactorOne);
}
