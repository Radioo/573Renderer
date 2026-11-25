#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "judge/afp_pose.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

std::array<float, 6> Rotation(float radians, float scale, float tx, float ty) {
    return {scale * std::cos(radians),
            scale * std::sin(radians),
            -scale * std::sin(radians),
            scale * std::cos(radians),
            tx,
            ty};
}

float AngleOf(const std::array<float, 6>& m) {
    return std::atan2(m[1], m[0]);
}

float XScaleOf(const std::array<float, 6>& m) {
    return std::hypot(m[0], m[1]);
}

AfpPose::Pose PoseWith(const std::array<float, 6>& matrix) {
    AfpPose::Pose pose;
    pose.character = 7;
    pose.depth = 3;
    pose.matrix = matrix;
    return pose;
}

}

TEST_CASE("Both lerp modes land exactly on the two authored poses at t = 0 and t = 1") {
    const auto from = PoseWith(Rotation(0.3F, 2.0F, 10.0F, 20.0F));
    const auto to = PoseWith(Rotation(1.1F, 0.5F, 40.0F, -5.0F));
    for (const auto mode : {AfpPose::MatrixLerp::Decompose, AfpPose::MatrixLerp::Linear}) {
        const auto start = AfpPose::Lerp(from, to, 0.0F, mode);
        const auto end = AfpPose::Lerp(from, to, 1.0F, mode);
        for (std::size_t i = 0; i < start.matrix.size(); i++) {
            CHECK(start.matrix.at(i) == Catch::Approx(from.matrix.at(i)).margin(1e-5));
            CHECK(end.matrix.at(i) == Catch::Approx(to.matrix.at(i)).margin(1e-5));
        }
    }
}

TEST_CASE("Decompose keeps the scale of a pure rotation, linear shrinks it") {
    const auto from = Rotation(0.0F, 1.0F, 0.0F, 0.0F);
    const auto to = Rotation(kPi / 2.0F, 1.0F, 0.0F, 0.0F);
    const auto mid = AfpPose::LerpMatrix(from, to, 0.5F, AfpPose::MatrixLerp::Decompose);
    CHECK(XScaleOf(mid) == Catch::Approx(1.0F));
    CHECK(AngleOf(mid) == Catch::Approx(kPi / 4.0F));
    const auto linear = AfpPose::LerpMatrix(from, to, 0.5F, AfpPose::MatrixLerp::Linear);
    CHECK(XScaleOf(linear) == Catch::Approx(std::sqrt(0.5F)));
}

TEST_CASE("Decompose turns the short way round across the half turn") {
    const auto from = Rotation(kPi - 0.1F, 1.0F, 0.0F, 0.0F);
    const auto to = Rotation(-kPi + 0.1F, 1.0F, 0.0F, 0.0F);
    const auto mid = AfpPose::LerpMatrix(from, to, 0.5F, AfpPose::MatrixLerp::Decompose);
    CHECK(std::fabs(AngleOf(mid)) == Catch::Approx(kPi));
}

TEST_CASE("A zero scale end carries no angle, so the other end's rotation is kept") {
    const auto from = Rotation(0.0F, 0.0F, 0.0F, 0.0F);
    const auto to = Rotation(kPi / 2.0F, 1.0F, 0.0F, 0.0F);
    const auto mid = AfpPose::LerpMatrix(from, to, 0.5F, AfpPose::MatrixLerp::Decompose);
    CHECK(XScaleOf(mid) == Catch::Approx(0.5F));
    CHECK(AngleOf(mid) == Catch::Approx(kPi / 2.0F));
}

TEST_CASE("A missing colour transform blends as identity and the anchor moves linearly") {
    auto from = PoseWith(Rotation(0.0F, 1.0F, 0.0F, 0.0F));
    auto to = from;
    to.has_color = true;
    to.color = {0.0F, 0.5F, 1.0F, 0.0F, 0.2F, 0.0F, 0.0F, 0.0F};
    from.anchor = {0.0F, 10.0F};
    to.anchor = {4.0F, 20.0F};
    const auto mid = AfpPose::Lerp(from, to, 0.5F, AfpPose::MatrixLerp::Decompose);
    REQUIRE(mid.has_color);
    CHECK(mid.color[0] == Catch::Approx(0.5F));
    CHECK(mid.color[1] == Catch::Approx(0.75F));
    CHECK(mid.color[3] == Catch::Approx(0.5F));
    CHECK(mid.color[4] == Catch::Approx(0.1F));
    CHECK(mid.anchor[0] == Catch::Approx(2.0F));
    CHECK(mid.anchor[1] == Catch::Approx(15.0F));
}

TEST_CASE("The 3D matrix and the camera blend element by element") {
    auto from = PoseWith(Rotation(0.0F, 1.0F, 0.0F, 0.0F));
    from.has_matrix3d = true;
    from.has_camera = true;
    auto to = from;
    from.matrix3d[12] = 0.0F;
    to.matrix3d[12] = 100.0F;
    from.camera = {500.0F, 0.0F, 0.0F, 0.0F, 400.0F, 300.0F};
    to.camera = {700.0F, 0.0F, 0.0F, 0.0F, 400.0F, 300.0F};
    const auto mid = AfpPose::Lerp(from, to, 0.25F, AfpPose::MatrixLerp::Decompose);
    CHECK(mid.matrix3d[12] == Catch::Approx(25.0F));
    CHECK(mid.camera[0] == Catch::Approx(550.0F));
    CHECK(AfpPose::CameraFov(mid.camera) == Catch::Approx(2.0F * std::atan2(200.0F, 550.0F)));
}

TEST_CASE("A character, depth, 3D or camera change is never interpolated") {
    const auto base = PoseWith(Rotation(0.0F, 1.0F, 0.0F, 0.0F));
    auto other = base;
    CHECK(AfpPose::Interpolable(base, other));
    other.character = 8;
    CHECK_FALSE(AfpPose::Interpolable(base, other));
    other = base;
    other.depth = 4;
    CHECK_FALSE(AfpPose::Interpolable(base, other));
    other = base;
    other.has_matrix3d = true;
    CHECK_FALSE(AfpPose::Interpolable(base, other));
    other = base;
    other.has_camera = true;
    CHECK_FALSE(AfpPose::Interpolable(base, other));
    other = base;
    other.has_color = true;
    CHECK(AfpPose::Interpolable(base, other));
}
