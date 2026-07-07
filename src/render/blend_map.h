#pragma once

#include <cstdint>

namespace Blend {

inline constexpr uint32_t kOpAdd = 1;
inline constexpr uint32_t kOpRevSubtract = 3;
inline constexpr uint32_t kOpMin = 4;
inline constexpr uint32_t kOpMax = 5;

inline constexpr uint32_t kFactorZero = 1;
inline constexpr uint32_t kFactorOne = 2;
inline constexpr uint32_t kFactorSrcColor = 3;
inline constexpr uint32_t kFactorSrcAlpha = 5;
inline constexpr uint32_t kFactorInvSrcAlpha = 6;

struct D3d9Blend {
    uint32_t op = kOpAdd;
    uint32_t src = kFactorSrcAlpha;
    uint32_t dst = kFactorInvSrcAlpha;
};

inline constexpr D3d9Blend kAlphaCoverage{kOpMax, kFactorOne, kFactorOne};

[[nodiscard]] bool IsAdditiveAfpMode(uint32_t mode);

[[nodiscard]] D3d9Blend MapAfpMode(uint32_t mode);

}
