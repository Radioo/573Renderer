#pragma once

#include "scene3d/camera.h"

#include <string>
#include <vector>

namespace Scene3dHost {

struct ModelInfo {
    std::string name;
    int blend_mode = 0;
    bool visible = true;
};

struct Status {
    std::string name;
    int models = 0;
    int tiles = 0;
    int draw_calls = 0;
    float time = 0.0F;
    float max_time = 0.0F;
    bool has_authored_camera = false;
    bool free_camera = false;
    bool paused = false;
    float speed = 1.0F;
    bool animate_models = true;
    bool animate_camera = true;
};

bool Load(const std::string& dir);

void Unload();

bool Active();

void RenderFrame(float dt);

Status GetStatus();

void SetFreeCamera(bool on);

void SetPaused(bool on);

void SetTime(float ticks);

void SetSpeed(float speed);

void ResetCamera();

void SetAnimateModels(bool on);

void SetAnimateCamera(bool on);

std::vector<ModelInfo> ListModels();

void SetModelVisible(int index, bool visible);

void SetModelBlend(int index, int mode);

Scene3d::FreeCamera& MutCamera();

}
