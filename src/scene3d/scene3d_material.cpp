#include "scene3d/scene3d_material.h"

#include <array>

namespace Scene3d {

MaterialColors MaterialFor(const std::array<float, 4>& diffuse,
                           const std::array<float, 3>& emissive, float alpha) {
    MaterialColors material;
    material.diffuse = {diffuse[0], diffuse[1], diffuse[2], diffuse[3] * alpha};
    material.ambient = {0.0F, 0.0F, 0.0F, 1.0F};
    material.emissive = {emissive[0], emissive[1], emissive[2], 1.0F};
    return material;
}

}
