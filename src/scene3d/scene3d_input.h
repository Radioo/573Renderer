#pragma once

#include "scene3d/camera.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Scene3d {

bool HandleLookMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

bool LookActive();

void PollCameraInput(CameraInput& out);

void SetInputEnabled(bool enabled);

}
