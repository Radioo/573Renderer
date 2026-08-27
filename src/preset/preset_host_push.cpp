#include "preset/preset_host_push.h"

#include "formats/gcanim.h"
#include "gc2d/gc_host.h"
#include "preset/eval/eval_push.h"
#include "preset/preset_host.h"
#include "scene3d/poly_grid.h"
#include "scene3d/scene3d_fog.h"
#include "scene3d/scene3d_host.h"
#include "scene3d/scene3d_render.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace PresetHost {

namespace {

using Preset::Eval::Push;
using Preset::Eval::PushCall;

std::uint32_t g_clear_color = 0;

std::uint32_t PackColor(const std::array<float, 3>& color) {
    std::uint32_t packed = 0;
    for (const float channel : color) {
        const auto level = (std::uint32_t)std::lround(std::clamp(channel, 0.0F, 1.0F) * 255.0F);
        packed = (packed << 8U) | level;
    }
    return packed;
}

Gc2dHost::SpritePlacement PlacementOf(const Preset::Eval::SpritePlacement& sprite) {
    return Gc2dHost::SpritePlacement{.asset = sprite.asset,
                                     .target = sprite.target,
                                     .name = sprite.name,
                                     .animated = sprite.animated,
                                     .priority = sprite.priority,
                                     .x = sprite.x,
                                     .y = sprite.y,
                                     .alpha = sprite.alpha,
                                     .scale = sprite.scale,
                                     .blend = (GcAnim::Blend)sprite.blend,
                                     .timing = sprite.timing,
                                     .skip_parts = sprite.skip_parts,
                                     .scroll_x = sprite.scroll_x,
                                     .scroll_wrap = sprite.scroll_wrap,
                                     .scroll_offset = sprite.scroll_offset};
}

void DrawParticleRuns(const std::vector<Preset::Eval::CellDraw>& cells) {
    std::vector<Gc2dHost::CellDraw> run;
    std::string asset;
    for (const Preset::Eval::CellDraw& cell : cells) {
        if (!run.empty() && cell.asset != asset) {
            Gc2dHost::DrawParticles(asset, run);
            run.clear();
        }
        asset = cell.asset;
        run.push_back(Gc2dHost::CellDraw{.name = cell.name,
                                         .x = cell.x,
                                         .y = cell.y,
                                         .alpha = cell.alpha,
                                         .scale = cell.scale,
                                         .blend = cell.blend});
    }
    if (!run.empty()) Gc2dHost::DrawParticles(asset, run);
}

bool ApplyFrameAnd2d(const Push& push) {
    switch (push.call) {
    case PushCall::DrawSprites:
        Gc2dHost::DrawSprites(push.index, push.index2);
        break;
    case PushCall::DrawParticles:
        DrawParticleRuns(push.cells);
        break;
    case PushCall::RenderFrame:
        Scene3dHost::RenderFrame(0.0F);
        break;
    case PushCall::AdvanceSprites:
        break;
    case PushCall::SetSprites: {
        std::vector<Gc2dHost::SpritePlacement> placements;
        placements.reserve(push.sprites.size());
        for (const Preset::Eval::SpritePlacement& sprite : push.sprites)
            placements.push_back(PlacementOf(sprite));
        Gc2dHost::SetSprites(std::move(placements));
        break;
    }
    case PushCall::SetSpriteScale:
        Gc2dHost::SetSpriteScale(push.index, push.value);
        break;
    case PushCall::SetSpriteFrame:
        Gc2dHost::SetSpriteFrame(push.index, push.index2);
        break;
    case PushCall::SetClearColor:
        g_clear_color = PackColor(push.vec_a);
        break;
    default:
        return false;
    }
    return true;
}

Scene3d::PolyGrid GridOf(const Preset::Eval::PolyGridPush& source, std::string movie) {
    Scene3d::PolyGrid grid;
    grid.active = source.active;
    grid.alpha = source.alpha;
    grid.seconds = source.seconds;
    grid.movie_width = source.movie_width;
    grid.movie_height = source.movie_height;
    grid.texture_side = source.texture_side;
    grid.movie = std::move(movie);
    grid.tiles.reserve(source.tiles.size());
    for (const Preset::Eval::PolyTilePush& tile : source.tiles)
        grid.tiles.push_back(Scene3d::PolyTile{.corners = tile.corners, .uv = tile.uv});
    return grid;
}

void ApplyLights(const std::vector<Preset::Eval::LightPush>& pushed) {
    std::vector<Scene3d::Light> lights;
    lights.reserve(pushed.size());
    for (const Preset::Eval::LightPush& light : pushed) {
        lights.push_back(Scene3d::Light{.direction = light.direction,
                                        .diffuse = light.diffuse,
                                        .specular = light.specular,
                                        .ambient = light.ambient,
                                        .enabled = light.enabled});
    }
    Scene3dHost::SetLights(lights);
}

void Apply3d(const Push& push) {
    switch (push.call) {
    case PushCall::SetStyle:
        Scene3dHost::SetStyle((Scene3d::RenderStyle)push.index);
        break;
    case PushCall::SetView:
        Scene3dHost::SetView(push.vec_a, push.vec_b, push.vec_c);
        break;
    case PushCall::SetProjection:
        Scene3dHost::SetProjection(Scene3d::Projection{.fov_y = push.projection.fov_y,
                                                       .near_z = push.projection.near_z,
                                                       .far_z = push.projection.far_z,
                                                       .aspect = push.projection.aspect});
        break;
    case PushCall::SetLights:
        ApplyLights(push.lights);
        break;
    case PushCall::SetModelAlpha:
        Scene3dHost::SetModelAlpha(push.name, push.value);
        break;
    case PushCall::SetModelSpeed:
        Scene3dHost::SetModelSpeed(push.name, push.value);
        break;
    case PushCall::SetModelBlend:
        Scene3dHost::SetModelBlendByName(push.name, push.index);
        break;
    case PushCall::SetModelScale:
        Scene3dHost::SetModelScale(push.name, push.vec_a);
        break;
    case PushCall::SetModelVisible:
        Scene3dHost::SetModelVisibleByName(push.name, push.flag);
        break;
    case PushCall::SetModelTransform:
        Scene3dHost::SetModelTransform(push.name, push.vec_a, push.vec_b);
        break;
    case PushCall::SetModelTime:
        Scene3dHost::SetModelTime(push.name, push.value);
        break;
    case PushCall::SetPolyGrid:
        Scene3dHost::SetPolyGrid(GridOf(
            push.poly, push.poly.movie.empty() ? std::string() : ResolveGameFile(push.poly.movie)));
        break;
    case PushCall::SetFog:
        Scene3dHost::SetFog(Scene3d::Fog{.enabled = push.fog.enabled,
                                         .color = push.fog.color,
                                         .start = push.fog.start,
                                         .end = push.fog.end,
                                         .density = push.fog.density});
        break;
    default:
        break;
    }
}

}

void ApplyPushes(const std::vector<Push>& pushes) {
    for (const Push& push : pushes) {
        if (!ApplyFrameAnd2d(push)) Apply3d(push);
    }
}

void ResetPushState() {
    g_clear_color = 0;
}

std::uint32_t ClearColor() {
    return g_clear_color;
}

}
