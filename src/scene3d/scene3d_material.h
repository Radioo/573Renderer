#pragma once

#include <array>

namespace Scene3d {

struct MaterialColors {
    std::array<float, 4> diffuse = {1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, 4> ambient = {0.0F, 0.0F, 0.0F, 1.0F};
    std::array<float, 4> emissive = {0.0F, 0.0F, 0.0F, 1.0F};
};

MaterialColors MaterialFor(const std::array<float, 4>& diffuse,
                           const std::array<float, 3>& emissive, float alpha);

}
