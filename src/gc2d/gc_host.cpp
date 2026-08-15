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
Gc2d::Package g_particles;
Gc2d::Renderer g_particle_renderer;
std::vector<GcAnim::DrawNode> g_scratch;

float ScrollOffset(const SpritePlacement& sprite) {
    if (sprite.scroll_wrap <= 0.0F) return 0.0F;
    return std::fmod(sprite.time * sprite.scroll_x, sprite.scroll_wrap);
}

float PlacedX(const SpritePlacement& sprite) {
    return sprite.x - ScrollOffset(sprite);
}

int SpriteLength(const SpritePlacement& sprite) {
    const auto it = g_pkg.index.animation_names.find(sprite.name);
    if (it == g_pkg.index.animation_names.end()) return 0;
    return SysIdx::AnimationLength(g_pkg.index, it->second);
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
    const int frame = GcAnim::ResolveFrame((int)sprite.time, length, sprite.timing);
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

void ScaleAbout(size_t from, const SpritePlacement& sprite) {
    if (sprite.scale == 1.0F) return;
    const float cx = sprite.x + 320.0F;
    const float cy = sprite.y + 240.0F;
    for (size_t i = from; i < g_nodes.size(); i++) {
        GcAnim::DrawNode& node = g_nodes[i];
        node.x = cx + ((node.x - cx) * sprite.scale);
        node.y = cy + ((node.y - cy) * sprite.scale);
        node.pivot_x = cx + ((node.pivot_x - cx) * sprite.scale);
        node.pivot_y = cy + ((node.pivot_y - cy) * sprite.scale);
        node.w *= sprite.scale;
        node.h *= sprite.scale;
    }
}

void EvaluateSprites(int min_priority, int max_priority) {
    g_nodes.clear();
    for (const auto& sprite : g_sprites) {
        if (sprite.priority < min_priority || sprite.priority > max_priority) continue;
        const size_t from = g_nodes.size();
        if (sprite.animated) {
            AppendAnimation(sprite);
        } else {
            AppendCell(sprite);
        }
        ScaleAbout(from, sprite);
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
    GcAnim::Evaluate(g_pkg.index, g_start, (int)g_time, 0.0F, 0.0F, g_nodes);
    DrawNodes();
    if (g_paused) return;

    g_time += dt * kFramesPerSecond * g_speed;
    const int length = CurrentLength();
    if (length > 0 && g_time >= (float)length) g_time -= (float)length;
}

void AdvanceSprites(float dt) {
    if (!g_active || g_paused) return;
    const float step = dt * kFramesPerSecond * g_speed;
    g_time += step;
    for (auto& sprite : g_sprites)
        sprite.time += step;
}

std::vector<SpriteStatus> ListSprites() {
    std::vector<SpriteStatus> out;
    out.reserve(g_sprites.size());
    for (const auto& sprite : g_sprites) {
        const int length = sprite.animated ? SpriteLength(sprite) : 0;
        out.push_back(
            SpriteStatus{.name = sprite.name,
                         .frame = GcAnim::ResolveFrame((int)sprite.time, length, sprite.timing),
                         .length = length,
                         .playhead = (int)sprite.time,
                         .scroll = (int)ScrollOffset(sprite),
                         .scroll_wrap = (int)sprite.scroll_wrap});
    }
    return out;
}

void SetSpriteFrame(int index, int frame) {
    if (index < 0 || (size_t)index >= g_sprites.size()) return;
    g_sprites[(size_t)index].time = (float)std::max(0, frame);
}

void SetSpriteScroll(int index, int offset) {
    if (index < 0 || (size_t)index >= g_sprites.size()) return;
    SpritePlacement& sprite = g_sprites[(size_t)index];
    if (sprite.scroll_wrap <= 0.0F || sprite.scroll_x == 0.0F) return;
    sprite.time = (float)std::max(0, offset) / sprite.scroll_x;
}

void SetSpriteScale(int index, float scale) {
    if (index < 0 || (size_t)index >= g_sprites.size()) return;
    g_sprites[(size_t)index].scale = scale;
}

std::vector<DrawInfo> ListDrawNodes() {
    std::vector<DrawInfo> out;
    out.reserve(g_nodes.size());
    for (const GcAnim::DrawNode& node : g_nodes) {
        DrawInfo info;
        for (const auto& [name, id] : g_pkg.index.cell_names) {
            if (std::cmp_equal(id, node.cell)) info.cell = name;
        }
        if (info.cell.empty()) info.cell = "#" + std::to_string(node.cell);
        info.blend = (int)node.blend;
        info.x = node.x;
        info.y = node.y;
        info.w = node.w;
        info.h = node.h;
        info.alpha = node.alpha;
        info.flags = node.flags;
        info.blend_code = node.blend_code;
        info.alpha_a = node.alpha_a;
        info.alpha_b = node.alpha_b;
        info.alpha_keys = node.alpha_keys;
        out.push_back(std::move(info));
    }
    return out;
}

bool LoadParticles(const std::string& dir) {
    if (g_particles.name == dir) return true;
    std::string err;
    Gc2d::Package package;
    if (!Gc2d::Load(dir, package, err)) {
        LOG("Gc2d", "particle package '%s' failed: %s", dir.c_str(), err.c_str());
        return false;
    }
    g_particle_renderer.Release();
    g_particles = std::move(package);
    if (!g_particle_renderer.Init(g_d3d.device, g_particles)) {
        LOG("Gc2d", "particle renderer init failed");
        return false;
    }
    LOG("Gc2d", "particle package '%s' ready: %zu cells", g_particles.name.c_str(),
        g_particles.index.cells.size());
    return true;
}

void DrawParticles(const std::vector<CellDraw>& cells) {
    if (cells.empty() || g_particles.index.cells.empty()) return;
    g_scratch.clear();
    for (const CellDraw& draw : cells) {
        const auto it = g_particles.index.cell_names.find(draw.name);
        if (it == g_particles.index.cell_names.end()) continue;
        if ((size_t)it->second >= g_particles.index.cells.size()) continue;
        const SysIdx::Cell& cell = g_particles.index.cells[it->second];
        const float w = (float)cell.w * draw.scale;
        const float h = (float)cell.h * draw.scale;
        GcAnim::DrawNode node;
        node.cell = it->second;
        node.x = draw.x - (w * 0.5F);
        node.y = draw.y - (h * 0.5F);
        node.w = w;
        node.h = h;
        node.pivot_x = node.x;
        node.pivot_y = node.y;
        node.alpha = draw.alpha;
        node.blend = (GcAnim::Blend)draw.blend;
        g_scratch.push_back(node);
    }
    if (g_scratch.empty()) return;
    int w = 0;
    int h = 0;
    g_d3d.GetOffscreenSize(w, h);
    if (w <= 0 || h <= 0) {
        w = g_d3d.width;
        h = g_d3d.height;
    }
    g_particle_renderer.Draw(g_particles, g_scratch, w, h);
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

namespace {

void CollectParts(size_t start, int depth, std::vector<std::string>& out) {
    if (depth > 8) return;
    for (size_t i = start; i < g_pkg.index.records.size(); i++) {
        const SysIdx::Record& rec = g_pkg.index.records[i];
        if (rec.type < 0) return;
        if (rec.type == SysIdx::kRecDrawCell) {
            for (const auto& [name, id] : g_pkg.index.cell_names) {
                if (std::cmp_equal(id, rec.id)) out.push_back("cell " + name);
            }
            continue;
        }
        if (rec.type != SysIdx::kRecNested) continue;
        for (const auto& [name, id] : g_pkg.index.animation_names) {
            if (std::cmp_equal(id, rec.id)) out.push_back("child " + name);
        }
        if (rec.id >= 0) CollectParts((size_t)rec.id, depth + 1, out);
    }
}

}

std::vector<std::string> ListParts(const std::string& animation) {
    std::vector<std::string> out;
    const auto it = g_pkg.index.animation_names.find(animation);
    if (it == g_pkg.index.animation_names.end()) return out;
    CollectParts(it->second, 0, out);
    std::ranges::sort(out);
    const auto dup = std::ranges::unique(out);
    out.erase(dup.begin(), dup.end());
    return out;
}

int AnimationLength(const std::string& animation) {
    const auto it = g_pkg.index.animation_names.find(animation);
    if (it == g_pkg.index.animation_names.end()) return 0;
    return SysIdx::AnimationLength(g_pkg.index, it->second);
}

std::vector<std::string> ListCells() {
    std::vector<std::string> names;
    names.reserve(g_pkg.index.cell_names.size());
    for (const auto& [name, id] : g_pkg.index.cell_names)
        names.push_back(name);
    std::ranges::sort(names);
    return names;
}

void SetSprites(std::vector<SpritePlacement> sprites) {
    const std::vector<SpritePlacement> previous = std::move(g_sprites);
    g_sprites = std::move(sprites);
    g_time = 0.0F;
    std::vector<bool> taken(previous.size(), false);
    for (auto& sprite : g_sprites) {
        sprite.time = 0.0F;
        for (size_t i = 0; i < previous.size(); i++) {
            if (taken[i] || previous[i].name != sprite.name) continue;
            sprite.time = previous[i].time;
            taken[i] = true;
            break;
        }
    }
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
