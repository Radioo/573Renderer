#pragma once

#include <array>
#include <cstdint>

namespace AfpPose {

enum class MatrixLerp : std::uint8_t { Decompose, Linear };

struct Pose {
    std::uint32_t character = 0;
    std::int32_t depth = 0;
    std::array<float, 6> matrix{};
    std::array<float, 2> anchor{};
    bool has_matrix3d = false;
    std::array<float, 16> matrix3d{};
    bool has_color = false;
    std::array<float, 8> color{};
    bool has_camera = false;
    std::array<float, 6> camera{};

    bool operator==(const Pose&) const = default;
};

[[nodiscard]] bool Interpolable(const Pose& from, const Pose& to);

[[nodiscard]] Pose Lerp(const Pose& from, const Pose& to, float t, MatrixLerp mode);

[[nodiscard]] std::array<float, 6> LerpMatrix(const std::array<float, 6>& from,
                                              const std::array<float, 6>& to, float t,
                                              MatrixLerp mode);

[[nodiscard]] float CameraFov(const std::array<float, 6>& camera);

[[nodiscard]] const char* MatrixLerpName(MatrixLerp mode);

}
