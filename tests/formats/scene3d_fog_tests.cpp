#include <catch2/catch_test_macros.hpp>

#include "scene3d/scene3d_fog.h"

#include <array>
#include <bit>
#include <cstdint>
#include <optional>

namespace {

std::optional<std::uint32_t> ValueOf(const std::array<Scene3d::FogWrite, Scene3d::kFogOnWrites>& on,
                                     std::uint32_t state) {
    for (const Scene3d::FogWrite& write : on) {
        if (write.state == state) return write.value;
    }
    return std::nullopt;
}

}

TEST_CASE("the fog bracket writes the render states HAPPY SKY's fog apply and fog defaults write "
          "between them") {
    const Scene3d::Fog fog{.enabled = true,
                           .color = {1.0F, 1.0F, 1.0F},
                           .start = 55.0F,
                           .end = 62.4F,
                           .density = 0.5F};
    const std::array<Scene3d::FogWrite, Scene3d::kFogOnWrites> on = Scene3d::FogOnWrites(fog);

    REQUIRE(ValueOf(on, 28).has_value());
    CHECK(*ValueOf(on, 28) == 1U);
    REQUIRE(ValueOf(on, 34).has_value());
    CHECK(*ValueOf(on, 34) == 0x00FFFFFFU);
    REQUIRE(ValueOf(on, 36).has_value());
    CHECK(std::bit_cast<float>(*ValueOf(on, 36)) == 55.0F);
    REQUIRE(ValueOf(on, 37).has_value());
    CHECK(std::bit_cast<float>(*ValueOf(on, 37)) == 62.4F);
    REQUIRE(ValueOf(on, 38).has_value());
    CHECK(std::bit_cast<float>(*ValueOf(on, 38)) == 0.5F);
    REQUIRE(ValueOf(on, 35).has_value());
    CHECK(*ValueOf(on, 35) == 0U);
    REQUIRE(ValueOf(on, 140).has_value());
    CHECK(*ValueOf(on, 140) == 3U);
    REQUIRE(ValueOf(on, 48).has_value());
    CHECK(*ValueOf(on, 48) == 1U);
}

TEST_CASE("the fog colour drops its alpha byte and the density is clamped to zero and one") {
    Scene3d::Fog fog{.enabled = true, .color = {0.0F, 48.0F / 255.0F, 96.0F / 255.0F}};
    fog.density = 2.0F;
    const std::array<Scene3d::FogWrite, Scene3d::kFogOnWrites> high = Scene3d::FogOnWrites(fog);
    REQUIRE(ValueOf(high, 34).has_value());
    CHECK(*ValueOf(high, 34) == 0x00003060U);
    REQUIRE(ValueOf(high, 38).has_value());
    CHECK(std::bit_cast<float>(*ValueOf(high, 38)) == 1.0F);

    fog.density = -1.0F;
    const std::array<Scene3d::FogWrite, Scene3d::kFogOnWrites> low = Scene3d::FogOnWrites(fog);
    REQUIRE(ValueOf(low, 38).has_value());
    CHECK(std::bit_cast<float>(*ValueOf(low, 38)) == 0.0F);
}

TEST_CASE("leaving the fog bracket clears the vertex mode, the table mode and the enable") {
    const std::array<Scene3d::FogWrite, Scene3d::kFogOffWrites> off = Scene3d::FogOffWrites();
    CHECK(off[0].state == 140U);
    CHECK(off[0].value == 0U);
    CHECK(off[1].state == 35U);
    CHECK(off[1].value == 0U);
    CHECK(off[2].state == 28U);
    CHECK(off[2].value == 0U);
}
