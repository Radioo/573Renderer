#include "gc2d/gc_host.h"

#include "app_globals.h"
#include "formats/gcanim.h"
#include "formats/sysidx.h"
#include "gc2d/gc_package.h"
#include "gc2d/gc_render.h"
#include "render_backend.h"
#include "state/app_state.h"
#include "support/log.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace Gc2dHost {

namespace {

constexpr float kFramesPerSecond = 60.0F;

Gc2d::Package g_pkg;
Gc2d::Renderer g_renderer;
std::vector<GcAnim::DrawNode> g_nodes;
std::string g_anim;
size_t g_start = 0;
bool g_active = false;
bool g_paused = false;
float g_time = 0.0F;
float g_speed = 1.0F;

int CurrentLength() {
    if (!g_active || g_pkg.index.records.empty()) return 0;
    return SysIdx::AnimationLength(g_pkg.index, g_start);
}

void PickFirstAnimation() {
    g_anim.clear();
    g_start = 0;
    if (g_pkg.animation_names.empty()) return;
    g_anim = g_pkg.animation_names.front();
    const auto it = g_pkg.index.animation_names.find(g_anim);
    if (it != g_pkg.index.animation_names.end()) g_start = it->second;
}

}

bool Load(const std::string& dir) {
    Unload();
    std::string err;
    if (!Gc2d::Load(dir, g_pkg, err)) {
        LOG("Gc2d", "load failed for %s: %s", dir.c_str(), err.c_str());
        return false;
    }
    if (!g_renderer.Init(g_d3d.device, g_pkg)) {
        LOG("Gc2d", "renderer init failed");
        return false;
    }
    PickFirstAnimation();
    g_time = 0.0F;
    g_paused = false;
    g_active = true;
    App::Global().SetActiveIfs(dir);
    LOG("Gc2d", "package '%s' ready, animation '%s' (%d frames)", g_pkg.name.c_str(),
        g_anim.c_str(), CurrentLength());
    return true;
}

void Unload() {
    if (!g_active) return;
    g_renderer.Release();
    g_pkg = Gc2d::Package{};
    g_nodes.clear();
    g_active = false;
}

bool Active() {
    return g_active;
}

void RenderFrame(float dt) {
    if (!g_active) return;
    if (!g_paused) g_time += dt * kFramesPerSecond * g_speed;

    const int length = CurrentLength();
    if (length > 0 && g_time >= (float)length) g_time -= (float)length;

    GcAnim::Evaluate(g_pkg.index, g_start, (int)g_time, 0.0F, 0.0F, g_nodes);

    int w = 0;
    int h = 0;
    g_d3d.GetOffscreenSize(w, h);
    if (w <= 0 || h <= 0) {
        w = g_d3d.width;
        h = g_d3d.height;
    }
    g_renderer.Draw(g_pkg, g_nodes, w, h);
}

Status GetStatus() {
    Status s;
    s.package = g_pkg.name;
    s.animation = g_anim;
    s.cells = (int)g_pkg.index.cells.size();
    s.records = (int)g_pkg.index.records.size();
    s.animations = (int)g_pkg.animation_names.size();
    s.tiles = (int)g_pkg.tiles.size();
    s.frame = (int)g_time;
    s.length = CurrentLength();
    s.draw_nodes = (int)g_nodes.size();
    s.paused = g_paused;
    s.speed = g_speed;
    return s;
}

std::vector<std::string> ListAnimations() {
    return g_pkg.animation_names;
}

void SelectAnimation(const std::string& name) {
    const auto it = g_pkg.index.animation_names.find(name);
    if (it == g_pkg.index.animation_names.end()) return;
    g_anim = name;
    g_start = it->second;
    g_time = 0.0F;
}

void SetPaused(bool on) {
    g_paused = on;
}

void SetFrame(int frame) {
    const int length = CurrentLength();
    g_time = (float)std::clamp(frame, 0, (length > 0) ? length - 1 : frame);
}

void SetSpeed(float speed) {
    g_speed = std::clamp(speed, 0.0F, 8.0F);
}

}
