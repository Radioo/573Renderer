#include "scene3d/scene3d_fog.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9types.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace Scene3d {

namespace {

std::uint32_t Bits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

std::uint32_t PackColor(const std::array<float, 3>& color) {
    std::uint32_t packed = 0;
    for (const float channel : color) {
        const auto level = (std::uint32_t)std::lround(std::clamp(channel, 0.0F, 1.0F) * 255.0F);
        packed = (packed << 8U) | level;
    }
    return packed & 0x00FFFFFFU;
}

}

std::array<FogWrite, kFogOnWrites> FogOnWrites(const Fog& fog) {
    return {FogWrite{.state = D3DRS_FOGENABLE, .value = TRUE},
            FogWrite{.state = D3DRS_FOGCOLOR, .value = PackColor(fog.color)},
            FogWrite{.state = D3DRS_FOGSTART, .value = Bits(fog.start)},
            FogWrite{.state = D3DRS_FOGEND, .value = Bits(fog.end)},
            FogWrite{.state = D3DRS_FOGDENSITY, .value = Bits(std::clamp(fog.density, 0.0F, 1.0F))},
            FogWrite{.state = D3DRS_FOGTABLEMODE, .value = D3DFOG_NONE},
            FogWrite{.state = D3DRS_FOGVERTEXMODE, .value = D3DFOG_LINEAR},
            FogWrite{.state = D3DRS_RANGEFOGENABLE, .value = TRUE}};
}

std::array<FogWrite, kFogOffWrites> FogOffWrites() {
    return {FogWrite{.state = D3DRS_FOGVERTEXMODE, .value = D3DFOG_NONE},
            FogWrite{.state = D3DRS_FOGTABLEMODE, .value = D3DFOG_NONE},
            FogWrite{.state = D3DRS_FOGENABLE, .value = FALSE}};
}

}
