#include "render/blend_map.h"

#include <cstdint>

namespace Blend {

bool IsAdditiveAfpMode(uint32_t mode) {
    return mode == 4 || mode == 8 || mode == 0x4F;
}

D3d9Blend MapAfpMode(uint32_t mode) {
    if (IsAdditiveAfpMode(mode)) {
        return {.op = kOpAdd, .src = kFactorSrcAlpha, .dst = kFactorOne};
    }
    switch (mode) {
    case 3:
        return {.op = kOpAdd, .src = kFactorZero, .dst = kFactorSrcColor};
    case 5:
        return {.op = kOpMax, .src = kFactorSrcAlpha, .dst = kFactorOne};
    case 6:
        return {.op = kOpMin, .src = kFactorSrcAlpha, .dst = kFactorOne};
    case 9:
    case 0x46:
        return {.op = kOpRevSubtract, .src = kFactorSrcAlpha, .dst = kFactorOne};
    default:
        return {.op = kOpAdd, .src = kFactorSrcAlpha, .dst = kFactorInvSrcAlpha};
    }
}

}
