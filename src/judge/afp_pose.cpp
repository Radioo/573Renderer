#include "judge/afp_pose.h"

#include <cmath>
#include <cstddef>
#include <numbers>

namespace AfpPose {

namespace {

constexpr std::array<float, 8> kIdentityColor{1.0F, 1.0F, 1.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F};
constexpr std::size_t kCameraFocal = 0;
constexpr std::size_t kCameraHeight = 4;
constexpr float kNoScale = 1.0e-6F;

float Mix(float from, float to, float t) {
    return from + ((to - from) * t);
}

float MixAngle(float from, float to, float t) {
    constexpr float kTurn = 2.0F * std::numbers::pi_v<float>;
    const float delta = std::remainder(to - from, kTurn);
    return from + (delta * t);
}

struct Parts {
    float x_scale;
    float y_scale;
    float rotation;
    float skew;
};

Parts Decompose(const std::array<float, 6>& m) {
    const float rotation = std::atan2(m[1], m[0]);
    return {.x_scale = std::hypot(m[0], m[1]),
            .y_scale = std::hypot(m[2], m[3]),
            .rotation = rotation,
            .skew = std::atan2(-m[2], m[3]) - rotation};
}

void BorrowUndefinedAngles(Parts& side, const Parts& other) {
    if (side.x_scale < kNoScale) {
        side.rotation = other.rotation;
        side.skew = other.skew;
    } else if (side.y_scale < kNoScale) {
        side.skew = other.skew;
    }
}

std::array<float, 6> DecomposedLerp(const std::array<float, 6>& from,
                                    const std::array<float, 6>& to, float t) {
    Parts a = Decompose(from);
    Parts b = Decompose(to);
    BorrowUndefinedAngles(a, b);
    BorrowUndefinedAngles(b, a);
    const float x_scale = Mix(a.x_scale, b.x_scale, t);
    const float y_scale = Mix(a.y_scale, b.y_scale, t);
    const float rotation = MixAngle(a.rotation, b.rotation, t);
    const float skew = MixAngle(a.skew, b.skew, t);
    return {x_scale * std::cos(rotation),
            x_scale * std::sin(rotation),
            -y_scale * std::sin(rotation + skew),
            y_scale * std::cos(rotation + skew),
            Mix(from[4], to[4], t),
            Mix(from[5], to[5], t)};
}

template <std::size_t N>
std::array<float, N> LinearLerp(const std::array<float, N>& from, const std::array<float, N>& to,
                                float t) {
    std::array<float, N> out{};
    for (std::size_t i = 0; i < N; ++i)
        out[i] = Mix(from[i], to[i], t);
    return out;
}

}

bool Interpolable(const Pose& from, const Pose& to) {
    return from.character == to.character && from.depth == to.depth &&
           from.has_matrix3d == to.has_matrix3d && from.has_camera == to.has_camera;
}

std::array<float, 6> LerpMatrix(const std::array<float, 6>& from, const std::array<float, 6>& to,
                                float t, MatrixLerp mode) {
    return mode == MatrixLerp::Decompose ? DecomposedLerp(from, to, t) : LinearLerp(from, to, t);
}

Pose Lerp(const Pose& from, const Pose& to, float t, MatrixLerp mode) {
    Pose out = to;
    out.matrix = LerpMatrix(from.matrix, to.matrix, t, mode);
    out.anchor = LinearLerp(from.anchor, to.anchor, t);
    if (to.has_matrix3d) out.matrix3d = LinearLerp(from.matrix3d, to.matrix3d, t);
    out.has_color = from.has_color || to.has_color;
    if (out.has_color) {
        out.color = LinearLerp(from.has_color ? from.color : kIdentityColor,
                               to.has_color ? to.color : kIdentityColor, t);
    }
    if (to.has_camera) out.camera = LinearLerp(from.camera, to.camera, t);
    return out;
}

float CameraFov(const std::array<float, 6>& camera) {
    return 2.0F * std::atan2(camera[kCameraHeight] * 0.5F, camera[kCameraFocal]);
}

const char* MatrixLerpName(MatrixLerp mode) {
    return mode == MatrixLerp::Decompose ? "decompose" : "linear";
}

}
