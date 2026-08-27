#include <catch2/catch_test_macros.hpp>

#include "scene3d/scene3d_material.h"

#include <array>

TEST_CASE("the model material keeps the D3DX8 loader's black ambient so the per-light ambient "
          "channel stays inert, as it is in the game") {
    const std::array<float, 4> diffuse = {0.5F, 0.25F, 1.0F, 0.8F};
    const std::array<float, 3> emissive = {0.1F, 0.2F, 0.3F};
    const Scene3d::MaterialColors material = Scene3d::MaterialFor(diffuse, emissive, 0.5F);
    CHECK(material.ambient == std::array<float, 4>{0.0F, 0.0F, 0.0F, 1.0F});
    CHECK(material.diffuse == std::array<float, 4>{0.5F, 0.25F, 1.0F, 0.4F});
    CHECK(material.emissive == std::array<float, 4>{0.1F, 0.2F, 0.3F, 1.0F});
}
