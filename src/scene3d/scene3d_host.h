#pragma once

#include "scene3d/camera.h"
#include "scene3d/scene3d_render.h"

#include <array>
#include <string>
#include <vector>

namespace Scene3dHost {

struct ModelInfo {
    std::string name;
    int blend_mode = 0;
    bool visible = true;
    float time = 0.0F;
    std::array<float, 3> position = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> rotation = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> scale = {1.0F, 1.0F, 1.0F};
};

struct ModelSetup {
    std::string model;
    int blend_mode = 0;
    float alpha = 1.0F;
    float anim_speed = 1.0F;
    std::array<float, 3> position = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> rotation = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> scale = {1.0F, 1.0F, 1.0F};
};

struct Setup {
    float ticks_per_second = 60.0F;
    std::vector<ModelSetup> models;
    std::vector<Scene3d::Light> lights;
    Scene3d::Projection projection;
    Scene3d::RenderStyle style = Scene3d::RenderStyle::TextureOnly;
    std::array<float, 3> eye = {0.0F, 0.0F, -1.0F};
    std::array<float, 3> at = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> up = {0.0F, 1.0F, 0.0F};
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

bool LoadWithSetup(const std::string& dir, const Setup& setup);

bool LoadUnion(const std::vector<std::string>& dirs, const Setup& setup);

struct SceneInfo {
    bool loaded = false;
    float max_time = 0.0F;
    std::vector<std::string> models;
};

SceneInfo DescribeScene(const std::string& dir);

void SetModelTime(const std::string& model, float ticks);

void SetModelSpeed(const std::string& model, float speed);

void SetModelAlpha(const std::string& model, float alpha);

void SetModelBlendByName(const std::string& model, int mode);

void SetModelScale(const std::string& model, const std::array<float, 3>& scale);

void SetModelVisibleByName(const std::string& model, bool visible);

void SetProjection(const Scene3d::Projection& projection);

void SetView(const std::array<float, 3>& eye, const std::array<float, 3>& at,
             const std::array<float, 3>& up);

void SetStyle(Scene3d::RenderStyle style);

void SetLights(const std::vector<Scene3d::Light>& lights);

void SetModelTransform(const std::string& model, const std::array<float, 3>& position,
                       const std::array<float, 3>& rotation);

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
