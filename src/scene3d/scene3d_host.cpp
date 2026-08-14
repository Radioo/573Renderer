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

constexpr float kViewerTicksPerSecond = 600.0F;

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
float g_tick_rate = kViewerTicksPerSecond;

bool g_fixed_camera = false;
XFile::Matrix g_fixed_view = XFile::Identity();

Scene3d::Model* FindModel(const std::string& name) {
    for (auto& m : g_scene.models) {
        if (m.name == name) return &m;
    }
    return nullptr;
}

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
    g_fixed_camera = false;
    g_tick_rate = kViewerTicksPerSecond;
    g_renderer.SetStyle(Scene3d::RenderStyle::TextureOnly);
    g_renderer.SetLights({});
    g_renderer.SetProjection(Scene3d::Projection{});
    Scene3d::SetInputEnabled(true);
    App::Global().SetActiveIfs(dir);
    LOG("Scene3d", "scene '%s' ready (free camera %s)", g_scene.name.c_str(),
        g_camera.active ? "on" : "off");
    return true;
}

bool LoadWithSetup(const std::string& dir, const Setup& setup) {
    if (!Load(dir)) return false;
    for (auto& model : g_scene.models)
        model.visible = false;
    for (const auto& want : setup.models) {
        Scene3d::Model* model = FindModel(want.model);
        if (model == nullptr) {
            LOG("Scene3d", "setup names model '%s' which is not in %s", want.model.c_str(),
                dir.c_str());
            continue;
        }
        model->visible = true;
        model->blend_mode = want.blend_mode;
        model->alpha = want.alpha;
        model->anim_speed = want.anim_speed;
        model->position = want.position;
        model->rotation = want.rotation;
        model->scale = want.scale;
        model->time = 0.0F;
    }
    g_tick_rate = setup.ticks_per_second;
    g_renderer.SetStyle(setup.style);
    g_renderer.SetLights(setup.lights);
    g_renderer.SetProjection(setup.projection);
    g_fixed_view = Scene3d::LookAtView(setup.eye.data(), setup.at.data(), setup.up.data());
    g_fixed_camera = true;
    g_camera.active = false;
    Scene3d::SetInputEnabled(false);
    return true;
}

void SetModelSpeed(const std::string& model, float speed) {
    Scene3d::Model* m = FindModel(model);
    if (m != nullptr) m->anim_speed = speed;
}

void SetModelAlpha(const std::string& model, float alpha) {
    Scene3d::Model* m = FindModel(model);
    if (m != nullptr) m->alpha = alpha;
}

void SetModelTransform(const std::string& model, const std::array<float, 3>& position,
                       const std::array<float, 3>& rotation) {
    Scene3d::Model* m = FindModel(model);
    if (m == nullptr) return;
    m->position = position;
    m->rotation = rotation;
}

void SetModelBlendByName(const std::string& model, int mode) {
    Scene3d::Model* m = FindModel(model);
    if (m != nullptr) m->blend_mode = mode;
}

void SetModelScale(const std::string& model, const std::array<float, 3>& scale) {
    Scene3d::Model* m = FindModel(model);
    if (m != nullptr) m->scale = scale;
}

void SetModelVisibleByName(const std::string& model, bool visible) {
    Scene3d::Model* m = FindModel(model);
    if (m != nullptr) m->visible = visible;
}

void SetProjection(const Scene3d::Projection& projection) {
    g_renderer.SetProjection(projection);
}

void SetView(const std::array<float, 3>& eye, const std::array<float, 3>& at,
             const std::array<float, 3>& up) {
    g_fixed_view = Scene3d::LookAtView(eye.data(), at.data(), up.data());
    g_fixed_camera = true;
}

void SetStyle(Scene3d::RenderStyle style) {
    g_renderer.SetStyle(style);
}

void SetLights(const std::vector<Scene3d::Light>& lights) {
    g_renderer.SetLights(lights);
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
        g_time += dt * g_tick_rate * g_speed;
        if (g_scene.max_time > 0.0F && g_time > g_scene.max_time) g_time = 0.0F;
        if (g_animate_models) {
            for (auto& model : g_scene.models) {
                model.time += dt * g_tick_rate * g_speed * model.anim_speed;
                if (g_scene.max_time > 0.0F && model.time > g_scene.max_time) model.time = 0.0F;
            }
        }
    }

    int w = 0;
    int h = 0;
    g_d3d.GetOffscreenSize(w, h);
    if (w <= 0 || h <= 0) {
        w = g_d3d.width;
        h = g_d3d.height;
    }

    const float camera_time = g_animate_camera ? g_time : g_camera_hold;
    const XFile::Matrix view = Scene3d::FreeCameraView(g_camera);
    const XFile::Matrix* override_view = nullptr;
    if (g_camera.active) {
        override_view = &view;
    } else if (g_fixed_camera) {
        override_view = &g_fixed_view;
    }
    g_renderer.Draw(g_scene, camera_time, w, h, override_view);
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
    for (auto& model : g_scene.models)
        model.time = g_time;
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
    for (const auto& m : g_scene.models) {
        out.push_back({.name = m.name,
                       .blend_mode = m.blend_mode,
                       .visible = m.visible,
                       .time = m.time,
                       .position = m.position,
                       .rotation = m.rotation,
                       .scale = m.scale});
    }
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
