#pragma once

#include "formats/xfile.h"

namespace Scene3d {

struct FreeCamera {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    float yaw = 0.0F;
    float pitch = 0.0F;
    float speed = 1.0F;
    bool active = false;
};

struct CameraInput {
    bool forward = false;
    bool back = false;
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool fast = false;
    bool slow = false;
    float look_dx = 0.0F;
    float look_dy = 0.0F;
};

void UpdateFreeCamera(FreeCamera& cam, const CameraInput& in, float dt);

XFile::Matrix FreeCameraView(const FreeCamera& cam);

void PlaceFreeCamera(FreeCamera& cam, const float center[3], float radius);

}
