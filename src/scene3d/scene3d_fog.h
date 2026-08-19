#pragma once

#include <array>
#include <cstdint>

namespace Scene3d {

struct Fog {
    bool enabled = false;
    std::array<float, 3> color = {1.0F, 1.0F, 1.0F};
    float start = 0.0F;
    float end = 1.0F;
    float density = 0.5F;
};

struct FogWrite {
    std::uint32_t state = 0;
    std::uint32_t value = 0;
};

inline constexpr std::size_t kFogOnWrites = 8;
inline constexpr std::size_t kFogOffWrites = 3;

std::array<FogWrite, kFogOnWrites> FogOnWrites(const Fog& fog);

std::array<FogWrite, kFogOffWrites> FogOffWrites();

}
