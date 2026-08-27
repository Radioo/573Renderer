#include "scene3d/camera.h"

#include "formats/xfile.h"
#include "scene3d/anim.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Scene3d {

namespace {

constexpr float kPitchLimit = 1.5533431F;
constexpr float kLookScale = 0.0035F;
constexpr float kFastFactor = 5.0F;
constexpr float kSlowFactor = 0.2F;

void Normalize(std::array<float, 3>& v) {
    const float len = std::sqrt((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]));
    if (len <= 1e-8F) return;
    for (float& c : v)
        c /= len;
}

}

void UpdateFreeCamera(FreeCamera& cam, const CameraInput& in, float dt) {
    cam.yaw += in.look_dx * kLookScale;
    cam.pitch += in.look_dy * kLookScale;
    cam.pitch = std::clamp(cam.pitch, -kPitchLimit, kPitchLimit);

    const float sy = std::sin(cam.yaw);
    const float cy = std::cos(cam.yaw);
    const float sp = std::sin(cam.pitch);
    const float cp = std::cos(cam.pitch);

    const float fx = sy * cp;
    const float fy = -sp;
    const float fz = cy * cp;
    const float rx = cy;
    const float rz = -sy;

    float step = cam.speed * dt;
    if (in.fast) step *= kFastFactor;
    if (in.slow) step *= kSlowFactor;

    float mx = 0.0F;
    float my = 0.0F;
    float mz = 0.0F;
    if (in.forward) {
        mx += fx;
        my += fy;
        mz += fz;
    }
    if (in.back) {
        mx -= fx;
        my -= fy;
        mz -= fz;
    }
    if (in.right) {
        mx += rx;
        mz += rz;
    }
    if (in.left) {
        mx -= rx;
        mz -= rz;
    }
    if (in.up) my += 1.0F;
    if (in.down) my -= 1.0F;

    const float len = std::sqrt((mx * mx) + (my * my) + (mz * mz));
    if (len > 1e-6F) {
        cam.x += (mx / len) * step;
        cam.y += (my / len) * step;
        cam.z += (mz / len) * step;
    }
}

XFile::Matrix FreeCameraView(const FreeCamera& cam) {
    XFile::Matrix translate = XFile::Identity();
    translate[12] = -cam.x;
    translate[13] = -cam.y;
    translate[14] = -cam.z;

    const float sy = std::sin(-cam.yaw);
    const float cy = std::cos(-cam.yaw);
    XFile::Matrix rot_y = XFile::Identity();
    rot_y[0] = cy;
    rot_y[2] = -sy;
    rot_y[8] = sy;
    rot_y[10] = cy;

    const float sp = std::sin(-cam.pitch);
    const float cp = std::cos(-cam.pitch);
    XFile::Matrix rot_x = XFile::Identity();
    rot_x[5] = cp;
    rot_x[6] = sp;
    rot_x[9] = -sp;
    rot_x[10] = cp;

    return Multiply(translate, Multiply(rot_y, rot_x));
}

XFile::Matrix LookAtView(const float eye[3], const float at[3], const float up[3]) {
    std::array<float, 3> z = {at[0] - eye[0], at[1] - eye[1], at[2] - eye[2]};
    Normalize(z);
    std::array<float, 3> x = {(up[1] * z[2]) - (up[2] * z[1]), (up[2] * z[0]) - (up[0] * z[2]),
                              (up[0] * z[1]) - (up[1] * z[0])};
    Normalize(x);
    const std::array<float, 3> y = {(z[1] * x[2]) - (z[2] * x[1]), (z[2] * x[0]) - (z[0] * x[2]),
                                    (z[0] * x[1]) - (z[1] * x[0])};

    XFile::Matrix m = XFile::Identity();
    m[0] = x[0];
    m[1] = y[0];
    m[2] = z[0];
    m[4] = x[1];
    m[5] = y[1];
    m[6] = z[1];
    m[8] = x[2];
    m[9] = y[2];
    m[10] = z[2];
    m[12] = -((x[0] * eye[0]) + (x[1] * eye[1]) + (x[2] * eye[2]));
    m[13] = -((y[0] * eye[0]) + (y[1] * eye[1]) + (y[2] * eye[2]));
    m[14] = -((z[0] * eye[0]) + (z[1] * eye[1]) + (z[2] * eye[2]));
    return m;
}

void PlaceFreeCamera(FreeCamera& cam, const float center[3], float radius) {
    const float dist = (radius > 0.0F) ? radius * 2.2F : 5.0F;
    cam.x = center[0];
    cam.y = center[1] + (dist * 0.25F);
    cam.z = center[2] - dist;
    cam.yaw = 0.0F;
    cam.pitch = 0.12F;
    cam.speed = (radius > 0.0F) ? radius : 2.0F;
}

}
