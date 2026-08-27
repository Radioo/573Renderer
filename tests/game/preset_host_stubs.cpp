#include "preset_host_stubs.h"

#include "preset_golden_format.h"

#include "formats/gcanim.h"
#include "gc2d/gc_host.h"
#include "gc2d/gc_sprite.h"
#include "scene3d/poly_grid.h"
#include "scene3d/scene3d_fog.h"
#include "scene3d/scene3d_host.h"
#include "scene3d/scene3d_render.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <ios>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<std::string> g_calls;
std::string g_error;
std::map<std::string, float> g_scene_ticks;
std::map<std::string, std::map<std::string, int>> g_package_frames;
std::map<std::string, int> g_animations;
float g_max_time = 0.0F;
int g_sprite_count = 0;
std::map<std::string, float> g_scene_max_time;
std::map<std::string, std::vector<std::string>> g_scene_models;
std::map<std::string, std::string> g_loaded_packages;
int g_canvas_width = 640;
int g_canvas_height = 480;
std::vector<Gc2dHost::SpritePlacement> g_placements;
std::vector<Scene3d::Light> g_lights;

std::string Key(const std::string& path) {
    std::string key = path;
    std::ranges::replace(key, '\\', '/');
    return key;
}

void Record(const std::string& name, const std::vector<std::string>& args) {
    g_calls.push_back(PresetGolden::FormatCall(name, args));
}

std::string Num(float value) {
    return PresetGolden::FormatFloat(value);
}

std::string Int(int value) {
    return std::to_string(value);
}

std::string Flag(bool value) {
    return value ? "true" : "false";
}

std::string Str(const std::string& value) {
    return "'" + value + "'";
}

std::string Vec(const std::array<float, 3>& value) {
    return "[" + Num(value[0]) + " " + Num(value[1]) + " " + Num(value[2]) + "]";
}

std::string ProjectionText(const Scene3d::Projection& projection) {
    return "proj[" + Num(projection.fov_y) + " " + Num(projection.near_z) + " " +
           Num(projection.far_z) + " " + Num(projection.aspect) + "]";
}

std::string FogText(const Scene3d::Fog& fog) {
    return "fog[" + Flag(fog.enabled) + " " + Vec(fog.color) + " " + Num(fog.start) + " " +
           Num(fog.end) + " " + Num(fog.density) + "]";
}

std::string LightText(const Scene3d::Light& light) {
    return "light[" + Vec(light.direction) + Vec(light.diffuse) + Vec(light.specular) +
           Vec(light.ambient) + "]";
}

std::string ModelText(const Scene3dHost::ModelSetup& model) {
    return "model[" + model.target + " " + model.model + " " + Int(model.blend_mode) + " " +
           Num(model.alpha) + " " + Num(model.anim_speed) + " " + Vec(model.position) +
           Vec(model.rotation) + Vec(model.scale) + "]";
}

std::string TimingText(const GcAnim::Timing& timing) {
    return "timing[" + Int((int)timing.playback) + " " + Int(timing.loop_start) + " " +
           Int(timing.loop_end) + "]";
}

std::string PartsText(const std::vector<std::string>& parts) {
    std::string out = "parts[";
    for (std::size_t i = 0; i < parts.size(); i++) {
        if (i > 0) out += " ";
        out += parts[i];
    }
    return out + "]";
}

std::string PlacementText(const Gc2dHost::SpritePlacement& sprite) {
    return "sprite[" + sprite.name + " " + Flag(sprite.animated) + " " + Int(sprite.priority) +
           " " + Num(sprite.x) + " " + Num(sprite.y) + " " + Num(sprite.alpha) + " " +
           Num(sprite.scale) + " " + Int((int)sprite.blend) + " " + TimingText(sprite.timing) +
           " " + PartsText(sprite.skip_parts) + " " + Num(sprite.scroll_x) + " " +
           Num(sprite.scroll_wrap) + "]";
}

std::string TileText(const Scene3d::PolyTile& tile) {
    std::string out = "tile[";
    for (std::size_t i = 0; i < tile.corners.size(); i++) {
        if (i > 0) out += " ";
        out += Vec(tile.corners[i]);
    }
    return out + "]";
}

std::string CellText(const Gc2dHost::CellDraw& cell) {
    return "cell[" + cell.name + " " + Num(cell.x) + " " + Num(cell.y) + " " + Num(cell.alpha) +
           " " + Num(cell.scale) + " " + Int(cell.blend) + "]";
}

void Fail(const std::string& message) {
    if (g_error.empty()) g_error = message;
}

}

namespace PresetStub {

bool LoadAssetLengths(const std::string& path, std::string& err) {
    const std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
        err = "could not open " + path;
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    const nlohmann::json doc = nlohmann::json::parse(text.str(), nullptr, false);
    if (doc.is_discarded() || !doc.is_object() || !doc.contains("scene3d") ||
        !doc.contains("package2d")) {
        err = path + " is not an object with scene3d and package2d";
        return false;
    }
    g_scene_ticks.clear();
    for (const auto& [dir, ticks] : doc["scene3d"].items())
        g_scene_ticks[dir] = ticks.get<float>();
    g_package_frames.clear();
    for (const auto& [dir, animations] : doc["package2d"].items()) {
        for (const auto& [name, frames] : animations.items())
            g_package_frames[dir][name] = frames.get<int>();
    }
    return true;
}

void Reset() {
    g_calls.clear();
    g_error.clear();
    g_animations.clear();
    g_max_time = 0.0F;
    g_sprite_count = 0;
    g_scene_max_time.clear();
    g_scene_models.clear();
    g_loaded_packages.clear();
    g_canvas_width = 640;
    g_canvas_height = 480;
    g_placements.clear();
    g_lights.clear();
}

Gc2d::Canvas Canvas() {
    return Gc2d::Canvas{.width = g_canvas_width, .height = g_canvas_height};
}

std::vector<Gc2dHost::SpritePlacement> Sprites() {
    return g_placements;
}

std::vector<Scene3d::Light> Lights() {
    return g_lights;
}

std::vector<std::string> Take() {
    std::vector<std::string> out = std::move(g_calls);
    g_calls.clear();
    return out;
}

const std::string& Error() {
    return g_error;
}

}

namespace Scene3dHost {

namespace {

bool LoadOne(const std::string& dir, const Setup& setup) {
    const std::string key = Key(dir);
    std::vector<std::string> args{
        Str(key),      Int((int)setup.style), Num(setup.ticks_per_second),     Vec(setup.eye),
        Vec(setup.at), Vec(setup.up),         ProjectionText(setup.projection)};
    for (const Scene3d::Light& light : setup.lights)
        args.push_back(LightText(light));
    for (const ModelSetup& model : setup.models)
        args.push_back(ModelText(model));
    Record("Scene3dHost::LoadWithSetup", args);
    const auto it = g_scene_ticks.find(key);
    if (it == g_scene_ticks.end()) {
        Fail("no scene3d max_time recorded for '" + key + "'");
        return false;
    }
    g_scene_max_time[key] = it->second;
    g_max_time = std::max(g_max_time, it->second);
    std::vector<std::string> models;
    models.reserve(setup.models.size());
    for (const ModelSetup& model : setup.models)
        models.push_back(model.model);
    g_scene_models[key] = std::move(models);
    return true;
}

}

bool LoadWithSetup(const std::string& dir, const Setup& setup) {
    return LoadOne(dir, setup);
}

bool LoadUnion(const std::vector<std::string>& dirs, const Setup& setup) {
    return std::ranges::all_of(dirs,
                               [&setup](const std::string& dir) { return LoadOne(dir, setup); });
}

SceneInfo DescribeScene(const std::string& dir) {
    const std::string key = Key(dir);
    const auto found = g_scene_max_time.find(key);
    if (found == g_scene_max_time.end()) return SceneInfo{};
    SceneInfo info;
    info.loaded = true;
    info.max_time = found->second;
    const auto models = g_scene_models.find(key);
    if (models != g_scene_models.end()) info.models = models->second;
    return info;
}

void SetModelTime(const std::string& model, float ticks) {
    Record("Scene3dHost::SetModelTime", {Str(model), Num(ticks)});
}

void Unload() {
    Record("Scene3dHost::Unload", {});
    g_max_time = 0.0F;
    g_scene_max_time.clear();
    g_scene_models.clear();
}

void RenderFrame(float dt) {
    Record("Scene3dHost::RenderFrame", {Num(dt)});
}

Status GetStatus() {
    Status status;
    status.max_time = g_max_time;
    return status;
}

void SetTime(float ticks) {
    Record("Scene3dHost::SetTime", {Num(ticks)});
}

void SetModelSpeed(const std::string& model, float speed) {
    Record("Scene3dHost::SetModelSpeed", {Str(model), Num(speed)});
}

void SetModelAlpha(const std::string& model, float alpha) {
    Record("Scene3dHost::SetModelAlpha", {Str(model), Num(alpha)});
}

void SetModelBlendByName(const std::string& model, int mode) {
    Record("Scene3dHost::SetModelBlendByName", {Str(model), Int(mode)});
}

void SetModelScale(const std::string& model, const std::array<float, 3>& scale) {
    Record("Scene3dHost::SetModelScale", {Str(model), Vec(scale)});
}

void SetModelVisibleByName(const std::string& model, bool visible) {
    Record("Scene3dHost::SetModelVisibleByName", {Str(model), Flag(visible)});
}

void SetModelTransform(const std::string& model, const std::array<float, 3>& position,
                       const std::array<float, 3>& rotation) {
    Record("Scene3dHost::SetModelTransform", {Str(model), Vec(position), Vec(rotation)});
}

void SetProjection(const Scene3d::Projection& projection) {
    Record("Scene3dHost::SetProjection", {ProjectionText(projection)});
}

void SetView(const std::array<float, 3>& eye, const std::array<float, 3>& at,
             const std::array<float, 3>& up) {
    Record("Scene3dHost::SetView", {Vec(eye), Vec(at), Vec(up)});
}

void SetStyle(Scene3d::RenderStyle style) {
    Record("Scene3dHost::SetStyle", {Int((int)style)});
}

void SetLights(const std::vector<Scene3d::Light>& lights) {
    std::vector<std::string> args;
    args.reserve(lights.size());
    for (const Scene3d::Light& light : lights)
        args.push_back(LightText(light));
    Record("Scene3dHost::SetLights", args);
    g_lights = lights;
}

void SetFog(const Scene3d::Fog& fog) {
    Record("Scene3dHost::SetFog", {FogText(fog)});
}

void SetPolyGrid(Scene3d::PolyGrid grid) {
    const Scene3d::PolyGrid placed = std::move(grid);
    std::vector<std::string> args{Flag(placed.active), Num(placed.alpha), Num(placed.seconds),
                                  Str(placed.movie), Int((int)placed.tiles.size())};
    for (const Scene3d::PolyTile& tile : placed.tiles)
        args.push_back(TileText(tile));
    Record("Scene3dHost::SetPolyGrid", args);
}

}

namespace Gc2dHost {

bool Load(const std::string& dir) {
    return LoadAsset({}, dir);
}

bool LoadAsset(const std::string& asset, const std::string& dir) {
    const std::string key = Key(dir);
    Record("Gc2dHost::LoadAsset", {Str(asset), Str(key)});
    const auto it = g_package_frames.find(key);
    g_animations = (it == g_package_frames.end()) ? std::map<std::string, int>{} : it->second;
    g_loaded_packages[asset] = key;
    return true;
}

PackageInfo DescribePackage(const std::string& asset) {
    PackageInfo info;
    const auto loaded = g_loaded_packages.find(asset);
    if (loaded == g_loaded_packages.end()) return info;
    const auto frames = g_package_frames.find(loaded->second);
    info.loaded = true;
    info.cells.emplace_back("PTC");
    if (frames == g_package_frames.end()) return info;
    for (const auto& [name, count] : frames->second)
        info.animations.emplace_back(AnimationInfo{.name = name, .frames = count, .parts = {}});
    return info;
}

void SetCanvas(int width, int height) {
    Record("Gc2dHost::SetCanvas", {Int(width), Int(height)});
    if (width <= 0 || height <= 0) return;
    g_canvas_width = width;
    g_canvas_height = height;
}

void Unload() {
    Record("Gc2dHost::Unload", {});
    g_animations.clear();
    g_sprite_count = 0;
    g_loaded_packages.clear();
    g_canvas_width = 640;
    g_canvas_height = 480;
}

int AnimationLength(const std::string& animation) {
    const auto it = g_animations.find(animation);
    if (it == g_animations.end()) {
        Fail("no 2D animation length recorded for '" + animation + "'");
        return 0;
    }
    return it->second;
}

void SetSprites(std::vector<SpritePlacement> sprites) {
    const std::vector<SpritePlacement> placed = std::move(sprites);
    std::vector<std::string> args;
    args.reserve(placed.size());
    for (const SpritePlacement& sprite : placed)
        args.push_back(PlacementText(sprite));
    Record("Gc2dHost::SetSprites", args);
    g_sprite_count = (int)placed.size();
    g_placements = placed;
}

std::vector<SpriteStatus> ListSprites() {
    return std::vector<SpriteStatus>((std::size_t)g_sprite_count);
}

void SetSpriteFrame(int index, int frame) {
    Record("Gc2dHost::SetSpriteFrame", {Int(index), Int(frame)});
}

void SetSpriteScale(int index, float scale) {
    Record("Gc2dHost::SetSpriteScale", {Int(index), Num(scale)});
}

void AdvanceSprites(float dt) {
    Record("Gc2dHost::AdvanceSprites", {Num(dt)});
}

void DrawSprites(int min_priority, int max_priority) {
    Record("Gc2dHost::DrawSprites", {Int(min_priority), Int(max_priority)});
}

void DrawParticles(const std::string& asset, const std::vector<CellDraw>& cells) {
    std::vector<std::string> args;
    args.push_back(Str(asset));
    args.reserve(cells.size());
    for (const CellDraw& cell : cells)
        args.push_back(CellText(cell));
    Record("Gc2dHost::DrawParticles", args);
}

void SetFrame(int frame) {
    Record("Gc2dHost::SetFrame", {Int(frame)});
}

}
