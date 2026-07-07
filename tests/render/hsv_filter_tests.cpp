#include <catch2/catch_test_macros.hpp>

#include "render/hsv_filter.h"

#include <array>
#include <cstring>

namespace {

std::array<unsigned char, 16> RawDesc(unsigned char id, unsigned short valid, float hue, float sat,
                                      float val) {
    std::array<unsigned char, 16> raw{};
    raw.at(0) = id;
    std::memcpy(&raw.at(2), &valid, sizeof(valid));
    std::memcpy(&raw.at(4), &hue, sizeof(hue));
    std::memcpy(&raw.at(8), &sat, sizeof(sat));
    std::memcpy(&raw.at(12), &val, sizeof(val));
    return raw;
}

}

TEST_CASE("ParseHsvDescriptor reads the afp 16-byte layout") {
    const auto raw = RawDesc(100, 1, 136.0F, -25.0F, 10.0F);
    const Render::HsvDescriptor d = Render::ParseHsvDescriptor(raw);
    CHECK(d.id == 100);
    CHECK(d.valid == 1);
    CHECK(d.hue == 136.0F);
    CHECK(d.sat == -25.0F);
    CHECK(d.val == 10.0F);
}

TEST_CASE("ParseHsvDescriptor rejects short buffers as inactive") {
    const std::array<unsigned char, 8> shorty{};
    const Render::HsvDescriptor d = Render::ParseHsvDescriptor(shorty);
    CHECK_FALSE(Render::HsvFilterActive(d));
}

TEST_CASE("HsvFilterActive gates on valid flag and the two filter ids") {
    CHECK(Render::HsvFilterActive(Render::ParseHsvDescriptor(RawDesc(100, 1, 0, 0, 0))));
    CHECK(Render::HsvFilterActive(Render::ParseHsvDescriptor(RawDesc(101, 1, 0, 0, 0))));
    CHECK_FALSE(Render::HsvFilterActive(Render::ParseHsvDescriptor(RawDesc(99, 1, 0, 0, 0))));
    CHECK_FALSE(Render::HsvFilterActive(Render::ParseHsvDescriptor(RawDesc(102, 1, 0, 0, 0))));
    CHECK_FALSE(Render::HsvFilterActive(Render::ParseHsvDescriptor(RawDesc(100, 0, 0, 0, 0))));
    CHECK_FALSE(Render::HsvFilterActive(Render::ParseHsvDescriptor(RawDesc(100, 2, 0, 0, 0))));
}

TEST_CASE("HsvShaderConstant feeds the YIQ rotation as the game does") {
    const Render::HsvDescriptor d =
        Render::ParseHsvDescriptor(RawDesc(100, 1, 90.0F, -25.0F, 0.0F));
    const std::array<float, 4> c = Render::HsvShaderConstant(d);
    CHECK(c.at(0) == 270.0F);
    CHECK(c.at(1) == 0.75F);
    CHECK(c.at(2) == 1.0F);
    CHECK(c.at(3) == 0.0F);
}
