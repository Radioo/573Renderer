#include "scene3d/scene3d_host.h"

#include "app_globals.h"
#include "state/app_state.h"
#include "formats/xfile.h"
#include "render_backend.h"
#include "scene3d/camera.h"
#include "scene3d/scene3d.h"
#include "scene3d/scene3d_input.h"
#include "scene3d/scene3d_render.h"
#include "support/log.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace Scene3dHost {

namespace {

constexpr float kTicksPerSecond = 600.0F;

Scene3d::Scene g_scene;
Scene3d::Renderer g_renderer;
Scene3d::FreeCamera g_camera;
bool g_active = false;
bool g_paused = false;
float g_time = 0.0F;
float g_speed = 1.0F;
bool g_animate_models = true;
bool g_animate_camera = true;
float g_model_hold = 0.0F;
float g_camera_hold = 0.0F;

void PlaceCameraFromBounds() {
    const std::array<float, 3> center = {(g_scene.bounds_min[0] + g_scene.bounds_max[0]) * 0.5F,
                                         (g_scene.bounds_min[1] + g_scene.bounds_max[1]) * 0.5F,
                                         (g_scene.bounds_min[2] + g_scene.bounds_max[2]) * 0.5F};
    float radius = 0.0F;
    for (size_t k = 0; k < 3; k++)
        radius = std::max(radius, (g_scene.bounds_max[k] - g_scene.bounds_min[k]) * 0.5F);
    Scene3d::PlaceFreeCamera(g_camera, center.data(), radius);
}

}

bool Load(const std::string& dir) {
    Unload();
    std::string err;
    if (!Scene3d::Load(dir, g_scene, err)) {
        LOG("Scene3d", "load failed for %s: %s", dir.c_str(), err.c_str());
        return false;
    }
    if (!g_renderer.Init(g_d3d.device, g_scene)) {
        LOG("Scene3d", "renderer init failed");
        return false;
    }
    PlaceCameraFromBounds();
    g_camera.active = g_scene.camera_model < 0;
    g_time = 0.0F;
    g_paused = false;
    g_animate_models = true;
    g_animate_camera = true;
    g_model_hold = 0.0F;
    g_camera_hold = 0.0F;
    g_active = true;
    Scene3d::SetInputEnabled(true);
    App::Global().SetActiveIfs(dir);
    LOG("Scene3d", "scene '%s' ready (free camera %s)", g_scene.name.c_str(),
        g_camera.active ? "on" : "off");
    return true;
}

void Unload() {
    if (!g_active) return;
    Scene3d::SetInputEnabled(false);
    g_renderer.Release();
    g_scene = Scene3d::Scene{};
    g_active = false;
}

bool Active() {
    return g_active;
}

void RenderFrame(float dt) {
    if (!g_active) return;

    Scene3d::CameraInput in;
    Scene3d::PollCameraInput(in);
    if (g_camera.active) Scene3d::UpdateFreeCamera(g_camera, in, dt);

    if (!g_paused) {
        g_time += dt * kTicksPerSecond * g_speed;
        if (g_scene.max_time > 0.0F && g_time > g_scene.max_time) g_time = 0.0F;
    }

    int w = 0;
    int h = 0;
    g_d3d.GetOffscreenSize(w, h);
    if (w <= 0 || h <= 0) {
        w = g_d3d.width;
        h = g_d3d.height;
    }

    const float model_time = g_animate_models ? g_time : g_model_hold;
    const float camera_time = g_animate_camera ? g_time : g_camera_hold;
    const XFile::Matrix view = Scene3d::FreeCameraView(g_camera);
    g_renderer.Draw(g_scene, model_time, camera_time, w, h, g_camera.active ? &view : nullptr);
}

Status GetStatus() {
    Status s;
    s.name = g_scene.name;
    s.models = (int)g_scene.models.size();
    s.tiles = (int)g_scene.tiles.size();
    s.draw_calls = g_renderer.DrawCalls();
    s.time = g_time;
    s.max_time = g_scene.max_time;
    s.has_authored_camera = g_scene.camera_model >= 0;
    s.free_camera = g_camera.active;
    s.paused = g_paused;
    s.speed = g_speed;
    s.animate_models = g_animate_models;
    s.animate_camera = g_animate_camera;
    return s;
}

void SetFreeCamera(bool on) {
    g_camera.active = on;
}

void SetPaused(bool on) {
    g_paused = on;
}

void SetTime(float ticks) {
    g_time = std::clamp(ticks, 0.0F, (g_scene.max_time > 0.0F) ? g_scene.max_time : ticks);
}

void SetSpeed(float speed) {
    g_speed = std::clamp(speed, 0.0F, 8.0F);
}

void ResetCamera() {
    PlaceCameraFromBounds();
}

void SetAnimateModels(bool on) {
    if (!on && g_animate_models) g_model_hold = g_time;
    g_animate_models = on;
}

void SetAnimateCamera(bool on) {
    if (!on && g_animate_camera) g_camera_hold = g_time;
    g_animate_camera = on;
}

std::vector<ModelInfo> ListModels() {
    std::vector<ModelInfo> out;
    out.reserve(g_scene.models.size());
    for (const auto& m : g_scene.models)
        out.push_back({.name = m.name, .blend_mode = m.blend_mode, .visible = m.visible});
    return out;
}

void SetModelVisible(int index, bool visible) {
    if (index < 0 || (size_t)index >= g_scene.models.size()) return;
    g_scene.models[(size_t)index].visible = visible;
}

void SetModelBlend(int index, int mode) {
    if (index < 0 || (size_t)index >= g_scene.models.size()) return;
    g_scene.models[(size_t)index].blend_mode = mode;
}

Scene3d::FreeCamera& MutCamera() {
    return g_camera;
}

}
