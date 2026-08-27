#include "gc2d/gc_host.h"

#include "app_globals.h"
#include "formats/gcanim.h"
#include "formats/sysidx.h"
#include "gc2d/gc_package.h"
#include "gc2d/gc_render.h"
#include "gc2d/gc_sprite.h"
#include "render_backend.h"
#include "state/app_state.h"
#include "support/log.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Gc2dHost {

namespace {

constexpr float kFramesPerSecond = 60.0F;

struct Loaded {
    std::string dir;
    Gc2d::Package pkg;
    Gc2d::Renderer renderer;
};

std::map<std::string, std::unique_ptr<Loaded>> g_assets;
Loaded* g_active = nullptr;
Gc2d::Canvas g_canvas;
std::vector<GcAnim::DrawNode> g_nodes;
std::vector<GcAnim::DrawNode> g_batch;
std::string g_anim;
size_t g_start = 0;
bool g_paused = false;
float g_time = 0.0F;
float g_speed = 1.0F;
std::vector<SpritePlacement> g_sprites;

Loaded* Find(const std::string& asset) {
    const auto it = g_assets.find(asset);
    return (it == g_assets.end()) ? nullptr : it->second.get();
}

int CurrentLength() {
    if (g_active == nullptr || g_active->pkg.index.records.empty()) return 0;
    return SysIdx::AnimationLength(g_active->pkg.index, g_start);
}

Gc2d::SpriteDraw DrawOf(const SpritePlacement& sprite) {
    return Gc2d::SpriteDraw{.name = sprite.name,
                            .animated = sprite.animated,
                            .x = sprite.x,
                            .y = sprite.y,
                            .alpha = sprite.alpha,
                            .scale = sprite.scale,
                            .blend = sprite.blend,
                            .timing = sprite.timing,
                            .skip_parts = sprite.skip_parts,
                            .time = sprite.time,
                            .scroll_x = sprite.scroll_x,
                            .scroll_wrap = sprite.scroll_wrap,
                            .scroll_offset = sprite.scroll_offset};
}

const std::string& CarryKey(const SpritePlacement& sprite) {
    return sprite.target.empty() ? sprite.name : sprite.target;
}

void TargetSize(int& w, int& h) {
    g_d3d.GetOffscreenSize(w, h);
    if (w > 0 && h > 0) return;
    w = g_d3d.width;
    h = g_d3d.height;
}

void DrawBatch(Loaded* owner, std::vector<GcAnim::DrawNode>& batch) {
    if (owner == nullptr || batch.empty()) {
        batch.clear();
        return;
    }
    g_nodes.insert(g_nodes.end(), batch.begin(), batch.end());
    int w = 0;
    int h = 0;
    TargetSize(w, h);
    owner->renderer.Draw(owner->pkg, batch, w, h, g_canvas);
    batch.clear();
}

void PickFirstAnimation() {
    g_anim.clear();
    g_start = 0;
    if (g_active == nullptr || g_active->pkg.animation_names.empty()) return;
    g_anim = g_active->pkg.animation_names.front();
    const auto it = g_active->pkg.index.animation_names.find(g_anim);
    if (it != g_active->pkg.index.animation_names.end()) g_start = it->second;
}

std::string ChildNames(const SysIdx::Package& index, size_t start) {
    std::string listed;
    for (size_t i = start; i < index.records.size(); i++) {
        const SysIdx::Record& rec = index.records[i];
        if (rec.type < 0) break;
        if (rec.type != SysIdx::kRecNested) continue;
        for (const auto& [name, child] : index.animation_names) {
            if (std::cmp_not_equal(child, rec.id)) continue;
            if (!listed.empty()) listed += ", ";
            listed += name;
        }
    }
    return listed;
}

void LogPlacement(const Loaded& owner, const SpritePlacement& sprite) {
    if (!sprite.animated) {
        if (!owner.pkg.index.cell_names.contains(sprite.name)) {
            LOG("Gc2d", "package '%s' has no cell named '%s'", owner.pkg.name.c_str(),
                sprite.name.c_str());
        }
        return;
    }
    const auto it = owner.pkg.index.animation_names.find(sprite.name);
    if (it == owner.pkg.index.animation_names.end()) {
        LOG("Gc2d", "package '%s' has no animation named '%s'", owner.pkg.name.c_str(),
            sprite.name.c_str());
        return;
    }
    LOG("Gc2d", "layer '%s': %d frames, children: %s", sprite.name.c_str(),
        SysIdx::AnimationLength(owner.pkg.index, it->second),
        ChildNames(owner.pkg.index, it->second).c_str());
}

}

bool LoadAsset(const std::string& asset, const std::string& dir) {
    Loaded* existing = Find(asset);
    if (existing != nullptr && existing->dir == dir) return true;
    auto entry = std::make_unique<Loaded>();
    entry->dir = dir;
    std::string err;
    if (!Gc2d::Load(dir, entry->pkg, err)) {
        LOG("Gc2d", "load failed for %s: %s", dir.c_str(), err.c_str());
        return false;
    }
    if (!entry->renderer.Init(g_d3d.device, entry->pkg)) {
        LOG("Gc2d", "renderer init failed for %s", dir.c_str());
        return false;
    }
    if (existing != nullptr) existing->renderer.Release();
    const bool was_active = (existing != nullptr) && (existing == g_active);
    Loaded* fresh = entry.get();
    g_assets[asset] = std::move(entry);
    if (g_active == nullptr || was_active) g_active = fresh;
    LOG("Gc2d", "package '%s' ready as asset '%s': %zu cells, %zu animations",
        fresh->pkg.name.c_str(), asset.c_str(), fresh->pkg.index.cells.size(),
        fresh->pkg.animation_names.size());
    return true;
}

bool Load(const std::string& dir) {
    Unload();
    if (!LoadAsset({}, dir)) return false;
    PickFirstAnimation();
    g_time = 0.0F;
    g_paused = false;
    App::Global().SetActiveIfs(dir);
    LOG("Gc2d", "package '%s' ready, animation '%s' (%d frames)", g_active->pkg.name.c_str(),
        g_anim.c_str(), CurrentLength());
    return true;
}

PackageInfo DescribePackage(const std::string& asset) {
    PackageInfo info;
    const Loaded* owner = Find(asset);
    if (owner == nullptr) return info;
    info.loaded = true;
    info.cells.reserve(owner->pkg.index.cell_names.size());
    for (const auto& [name, id] : owner->pkg.index.cell_names)
        info.cells.push_back(name);
    std::ranges::sort(info.cells);
    info.animations.reserve(owner->pkg.animation_names.size());
    for (const std::string& name : owner->pkg.animation_names) {
        const auto it = owner->pkg.index.animation_names.find(name);
        const int frames = (it == owner->pkg.index.animation_names.end())
                               ? 0
                               : SysIdx::AnimationLength(owner->pkg.index, it->second);
        info.animations.push_back(AnimationInfo{
            .name = name, .frames = frames, .parts = Gc2d::PartNames(owner->pkg.index, name)});
    }
    return info;
}

void SetCanvas(int width, int height) {
    if (width <= 0 || height <= 0) return;
    g_canvas = Gc2d::Canvas{.width = width, .height = height};
}

void Unload() {
    if (g_assets.empty()) return;
    for (auto& [asset, entry] : g_assets)
        entry->renderer.Release();
    g_assets.clear();
    g_active = nullptr;
    g_canvas = Gc2d::Canvas{};
    g_nodes.clear();
    g_batch.clear();
    g_sprites.clear();
    g_anim.clear();
    g_start = 0;
}

bool Active() {
    return g_active != nullptr;
}

void RenderFrame(float dt) {
    if (g_active == nullptr) return;
    g_nodes.clear();
    GcAnim::Evaluate(g_active->pkg.index, g_start, (int)g_time, 0.0F, 0.0F, g_nodes);
    int w = 0;
    int h = 0;
    TargetSize(w, h);
    g_active->renderer.Draw(g_active->pkg, g_nodes, w, h, g_canvas);
    if (g_paused) return;

    g_time += dt * kFramesPerSecond * g_speed;
    const int length = CurrentLength();
    if (length > 0 && g_time >= (float)length) g_time -= (float)length;
}

void AdvanceSprites(float dt) {
    if (g_active == nullptr || g_paused) return;
    const float step = dt * kFramesPerSecond * g_speed;
    g_time += step;
    for (auto& sprite : g_sprites)
        sprite.time += step;
}

std::vector<SpriteStatus> ListSprites() {
    std::vector<SpriteStatus> out;
    out.reserve(g_sprites.size());
    for (const auto& sprite : g_sprites) {
        const Loaded* owner = Find(sprite.asset);
        const Gc2d::SpriteDraw draw = DrawOf(sprite);
        const int length =
            (sprite.animated && owner != nullptr) ? Gc2d::SpriteLength(owner->pkg.index, draw) : 0;
        out.push_back(
            SpriteStatus{.name = sprite.name,
                         .frame = GcAnim::ResolveFrame((int)sprite.time, length, sprite.timing),
                         .length = length,
                         .playhead = (int)sprite.time,
                         .scroll = (int)Gc2d::ScrollOffset(draw),
                         .scroll_wrap = (int)sprite.scroll_wrap});
    }
    return out;
}

void SetSpriteFrame(int index, int frame) {
    if (index < 0 || (size_t)index >= g_sprites.size()) return;
    g_sprites[(size_t)index].time = (float)std::max(0, frame);
}

void SetSpriteScale(int index, float scale) {
    if (index < 0 || (size_t)index >= g_sprites.size()) return;
    g_sprites[(size_t)index].scale = scale;
}

std::vector<DrawInfo> ListDrawNodes() {
    std::vector<DrawInfo> out;
    if (g_active == nullptr) return out;
    out.reserve(g_nodes.size());
    for (const GcAnim::DrawNode& node : g_nodes) {
        DrawInfo info;
        for (const auto& [name, id] : g_active->pkg.index.cell_names) {
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

void DrawParticles(const std::string& asset, const std::vector<CellDraw>& cells) {
    Loaded* owner = Find(asset);
    if (cells.empty() || owner == nullptr || owner->pkg.index.cells.empty()) return;
    g_batch.clear();
    for (const CellDraw& draw : cells) {
        const auto it = owner->pkg.index.cell_names.find(draw.name);
        if (it == owner->pkg.index.cell_names.end()) continue;
        if ((size_t)it->second >= owner->pkg.index.cells.size()) continue;
        const SysIdx::Cell& cell = owner->pkg.index.cells[it->second];
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
        g_batch.push_back(node);
    }
    if (g_batch.empty()) return;
    int tw = 0;
    int th = 0;
    TargetSize(tw, th);
    owner->renderer.Draw(owner->pkg, g_batch, tw, th, g_canvas);
    g_batch.clear();
}

void DrawSprites(int min_priority, int max_priority) {
    if (g_assets.empty() || g_sprites.empty()) return;
    g_nodes.clear();
    g_batch.clear();
    Loaded* run = nullptr;
    for (const auto& sprite : g_sprites) {
        if (sprite.priority < min_priority || sprite.priority > max_priority) continue;
        Loaded* owner = Find(sprite.asset);
        if (owner == nullptr) continue;
        if (owner != run) DrawBatch(run, g_batch);
        run = owner;
        Gc2d::AppendNodes(owner->pkg.index, DrawOf(sprite), g_canvas, g_batch);
    }
    DrawBatch(run, g_batch);
}

Status GetStatus() {
    Status s;
    if (g_active == nullptr) return s;
    s.package = g_active->pkg.name;
    s.animation = g_anim;
    s.cells = (int)g_active->pkg.index.cells.size();
    s.records = (int)g_active->pkg.index.records.size();
    s.animations = (int)g_active->pkg.animation_names.size();
    s.tiles = (int)g_active->pkg.tiles.size();
    s.frame = (int)g_time;
    s.length = CurrentLength();
    s.draw_nodes = (int)g_nodes.size();
    s.paused = g_paused;
    s.speed = g_speed;
    return s;
}

std::vector<std::string> ListAnimations() {
    return (g_active == nullptr) ? std::vector<std::string>{} : g_active->pkg.animation_names;
}

std::vector<std::string> ListParts(const std::string& animation) {
    if (g_active == nullptr) return {};
    return Gc2d::PartNames(g_active->pkg.index, animation);
}

int AnimationLength(const std::string& animation) {
    if (g_active == nullptr) return 0;
    const auto it = g_active->pkg.index.animation_names.find(animation);
    if (it == g_active->pkg.index.animation_names.end()) return 0;
    return SysIdx::AnimationLength(g_active->pkg.index, it->second);
}

std::vector<std::string> ListCells() {
    if (g_active == nullptr) return {};
    std::vector<std::string> names;
    names.reserve(g_active->pkg.index.cell_names.size());
    for (const auto& [name, id] : g_active->pkg.index.cell_names)
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
            if (taken[i] || CarryKey(previous[i]) != CarryKey(sprite)) continue;
            sprite.time = previous[i].time;
            taken[i] = true;
            break;
        }
    }
    for (const auto& sprite : g_sprites) {
        const Loaded* owner = Find(sprite.asset);
        if (owner == nullptr) {
            LOG("Gc2d", "layer '%s' names asset '%s' which is not loaded", sprite.name.c_str(),
                sprite.asset.c_str());
            continue;
        }
        LogPlacement(*owner, sprite);
    }
}

bool SelectAnimation(const std::string& name) {
    if (g_active == nullptr) return false;
    const auto it = g_active->pkg.index.animation_names.find(name);
    if (it == g_active->pkg.index.animation_names.end()) return false;
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
