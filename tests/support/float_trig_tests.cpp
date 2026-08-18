#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "support/math/float_trig.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace {

struct Pin {
    std::uint32_t argument;
    std::uint32_t sine;
    std::uint32_t cosine;
};

constexpr std::array<Pin, 15> kPins = {{
    {.argument = 0x00000000U, .sine = 0x00000000U, .cosine = 0x3f800000U},
    {.argument = 0x3f800000U, .sine = 0x3f576aa4U, .cosine = 0x3f0a5140U},
    {.argument = 0x40490fdbU, .sine = 0xb3bbbd2eU, .cosine = 0xbf800000U},
    {.argument = 0x41c37778U, .sine = 0xbf24cdb6U, .cosine = 0x3f43e5bbU},
    {.argument = 0x41c33334U, .sine = 0xbf2b3d9aU, .cosine = 0x3f3e4bd5U},
    {.argument = 0x42c80000U, .sine = 0xbf01a12eU, .cosine = 0x3f5cc0eeU},
    {.argument = 0x4b189680U, .sine = 0x3ed7520aU, .cosine = 0xbf6842dfU},
    {.argument = 0x4b3c614eU, .sine = 0xbf674e79U, .cosine = 0xbedb6487U},
    {.argument = 0x60ad78ecU, .sine = 0x3f281569U, .cosine = 0x3f411723U},
    {.argument = 0xbf800000U, .sine = 0xbf576aa4U, .cosine = 0x3f0a5140U},
    {.argument = 0x3c23d70aU, .sine = 0x3c23d657U, .cosine = 0x3f7ffcb9U},
    {.argument = 0x7f7fffffU, .sine = 0xbf0599b3U, .cosine = 0x3f5a5f96U},
    {.argument = 0x7d685ba2U, .sine = 0xbf75644cU, .cosine = 0xbe91dc7aU},
    {.argument = 0xf72a0e1eU, .sine = 0x3f7b6ac3U, .cosine = 0x3e40e486U},
    {.argument = 0xec9e8f90U, .sine = 0xbe3c66c6U, .cosine = 0x3f7ba13aU},
}};

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

float FromBits(std::uint32_t bits) {
    return std::bit_cast<float>(bits);
}

float Gap(float a, float b) {
    return std::fabs(a - b);
}

}

TEST_CASE("the deterministic sine and cosine hold their exact float bits", "[ci]") {
    for (const Pin& pin : kPins) {
        const float x = FromBits(pin.argument);
        INFO("argument bits " << std::to_string(pin.argument));
        CHECK(Bits(Support::Sinf(x)) == pin.sine);
        CHECK(Bits(Support::Cosf(x)) == pin.cosine);
    }
}

TEST_CASE("the deterministic sine and cosine track the platform library", "[ci]") {
    constexpr float kTolerance = 4.0F * std::numeric_limits<float>::epsilon();
    constexpr std::uint32_t kLargePath = 0x4dc90fdbU;
    std::uint64_t seed = 0x12345678ULL;
    float worst = 0.0F;
    int compared = 0;
    for (int i = 0; i < 200000; i++) {
        seed = (seed * 6364136223846793005ULL) + 1442695040888963407ULL;
        const auto bits = (std::uint32_t)(seed >> 32U);
        if ((bits & 0x7fffffffU) >= kLargePath) continue;
        const float x = FromBits(bits);
        worst = std::max(worst, Gap(Support::Sinf(x), std::sin(x)));
        worst = std::max(worst, Gap(Support::Cosf(x), std::cos(x)));
        compared++;
    }
    INFO("compared " << std::to_string(compared) << " arguments, worst gap "
                     << std::to_string(worst) << " against a tolerance of "
                     << std::to_string(kTolerance));
    CHECK(compared > 100000);
    CHECK(worst <= kTolerance);
}
