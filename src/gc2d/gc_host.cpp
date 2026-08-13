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
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
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

std::vector<SpritePlacement> g_sprites;
std::vector<GcAnim::DrawNode> g_scratch;

float PlacedX(const SpritePlacement& sprite) {
    if (sprite.scroll_wrap <= 0.0F) return sprite.x;
    return sprite.x - std::fmod(g_time * sprite.scroll_x, sprite.scroll_wrap);
}

void AppendCell(const SpritePlacement& sprite) {
    const auto it = g_pkg.index.cell_names.find(sprite.name);
    if (it == g_pkg.index.cell_names.end()) return;
    if ((size_t)it->second >= g_pkg.index.cells.size()) return;
    const SysIdx::Cell& cell = g_pkg.index.cells[it->second];
    GcAnim::DrawNode node;
    node.cell = it->second;
    node.x = PlacedX(sprite);
    node.y = sprite.y;
    node.w = (float)cell.w;
    node.h = (float)cell.h;
    node.pivot_x = node.x;
    node.pivot_y = node.y;
    node.alpha = sprite.alpha;
    node.blend = sprite.blend;
    g_nodes.push_back(node);
}

std::string ChildNames(size_t start) {
    std::string listed;
    for (size_t i = start; i < g_pkg.index.records.size(); i++) {
        const SysIdx::Record& rec = g_pkg.index.records[i];
        if (rec.type < 0) break;
        if (rec.type != SysIdx::kRecNested) continue;
        for (const auto& [name, child] : g_pkg.index.animation_names) {
            if (std::cmp_not_equal(child, rec.id)) continue;
            if (!listed.empty()) listed += ", ";
            listed += name;
        }
    }
    return listed;
}

void AppendAnimation(const SpritePlacement& sprite) {
    const auto it = g_pkg.index.animation_names.find(sprite.name);
    if (it == g_pkg.index.animation_names.end()) return;
    const int length = SysIdx::AnimationLength(g_pkg.index, it->second);
    const int frame = GcAnim::ResolveFrame((int)g_time, length, sprite.timing);
    if (frame < 0) return;
    std::vector<size_t> skip_children;
    std::vector<int> skip_cells;
    for (const std::string& part : sprite.skip_parts) {
        const auto child = g_pkg.index.animation_names.find(part);
        if (child != g_pkg.index.animation_names.end()) skip_children.push_back(child->second);
        const auto cell = g_pkg.index.cell_names.find(part);
        if (cell != g_pkg.index.cell_names.end()) skip_cells.push_back(cell->second);
    }
    GcAnim::Evaluate(g_pkg.index, it->second, frame, PlacedX(sprite), sprite.y, g_scratch,
                     GcAnim::SkipSet{.children = skip_children, .cells = skip_cells});
    g_nodes.insert(g_nodes.end(), g_scratch.begin(), g_scratch.end());
}

void EvaluateSprites(int min_priority, int max_priority) {
    g_nodes.clear();
    for (const auto& sprite : g_sprites) {
        if (sprite.priority < min_priority || sprite.priority > max_priority) continue;
        if (sprite.animated) {
            AppendAnimation(sprite);
        } else {
            AppendCell(sprite);
        }
    }
}

void DrawNodes() {
    int w = 0;
    int h = 0;
    g_d3d.GetOffscreenSize(w, h);
    if (w <= 0 || h <= 0) {
        w = g_d3d.width;
        h = g_d3d.height;
    }
    g_renderer.Draw(g_pkg, g_nodes, w, h);
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
    g_sprites.clear();
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
    DrawNodes();
}

void AdvanceSprites(float dt) {
    if (!g_active || g_paused) return;
    g_time += dt * kFramesPerSecond * g_speed;
}

void DrawSprites(int min_priority, int max_priority) {
    if (!g_active || g_sprites.empty()) return;
    EvaluateSprites(min_priority, max_priority);
    if (g_nodes.empty()) return;
    DrawNodes();
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

void SetSprites(std::vector<SpritePlacement> sprites) {
    g_sprites = std::move(sprites);
    g_time = 0.0F;
    for (const auto& sprite : g_sprites) {
        if (!sprite.animated) {
            if (!g_pkg.index.cell_names.contains(sprite.name)) {
                LOG("Gc2d", "package '%s' has no cell named '%s'", g_pkg.name.c_str(),
                    sprite.name.c_str());
            }
            continue;
        }
        const auto it = g_pkg.index.animation_names.find(sprite.name);
        if (it == g_pkg.index.animation_names.end()) {
            LOG("Gc2d", "package '%s' has no animation named '%s'", g_pkg.name.c_str(),
                sprite.name.c_str());
            continue;
        }
        LOG("Gc2d", "layer '%s': %d frames, children: %s", sprite.name.c_str(),
            SysIdx::AnimationLength(g_pkg.index, it->second), ChildNames(it->second).c_str());
    }
}

bool SelectAnimation(const std::string& name) {
    const auto it = g_pkg.index.animation_names.find(name);
    if (it == g_pkg.index.animation_names.end()) return false;
    g_anim = name;
    g_start = it->second;
    g_time = 0.0F;
    return true;
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
