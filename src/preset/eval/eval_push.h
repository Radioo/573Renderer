#pragma once

#include "formats/gcanim.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Preset::Eval {

enum class PushCall : uint8_t {
    DrawSprites,
    DrawParticles,
    RenderFrame,
    AdvanceSprites,
    SetSprites,
    SetSpriteScale,
    SetSpriteFrame,
    SetStyle,
    SetView,
    SetProjection,
    SetLights,
    SetModelAlpha,
    SetModelSpeed,
    SetModelBlend,
    SetModelScale,
    SetModelVisible,
    SetModelTransform,
    SetModelTime,
    SetClearColor,
    SetFog,
    SetPolyGrid,
};

struct SpritePlacement {
    std::string asset;
    std::string target;
    std::string name;
    bool animated = false;
    int priority = 0;
    float x = 0.0F;
    float y = 0.0F;
    float alpha = 1.0F;
    float scale = 1.0F;
    int blend = 0;
    GcAnim::Timing timing = {};
    std::vector<std::string> skip_parts;
    float scroll_x = 0.0F;
    float scroll_wrap = 0.0F;
    float scroll_offset = 0.0F;
};

struct CellDraw {
    std::string asset;
    std::string name;
    float x = 0.0F;
    float y = 0.0F;
    float alpha = 1.0F;
    float scale = 1.0F;
    int blend = 0;
};

struct LightPush {
    std::array<float, 3> direction = {0.0F, 0.0F, -1.0F};
    std::array<float, 3> diffuse = {1.0F, 1.0F, 1.0F};
    std::array<float, 3> specular = {1.0F, 1.0F, 1.0F};
    std::array<float, 3> ambient = {0.0F, 0.0F, 0.0F};
    bool enabled = true;
};

struct ProjectionPush {
    float fov_y = 1.0471976F;
    float near_z = 0.1F;
    float far_z = 500.0F;
    float aspect = 0.0F;
};

struct FogPush {
    bool enabled = false;
    std::array<float, 3> color = {1.0F, 1.0F, 1.0F};
    float start = 0.0F;
    float end = 1.0F;
    float density = 0.5F;
};

struct PolyTilePush {
    std::array<std::array<float, 3>, 4> corners = {};
    std::array<std::array<float, 2>, 4> uv = {};
};

struct PolyGridPush {
    bool active = false;
    float alpha = 1.0F;
    float seconds = 0.0F;
    int movie_width = 0;
    int movie_height = 0;
    int texture_side = 0;
    std::string movie;
    std::vector<PolyTilePush> tiles;
};

struct Push {
    PushCall call = PushCall::RenderFrame;
    bool legacy = true;
    bool model_moves = true;
    std::string name;
    int index = 0;
    int index2 = 0;
    bool flag = false;
    float value = 0.0F;
    std::array<float, 3> vec_a = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> vec_b = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> vec_c = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> legacy_vec_b = {0.0F, 0.0F, 0.0F};
    ProjectionPush projection = {};
    FogPush fog = {};
    PolyGridPush poly = {};
    std::vector<SpritePlacement> sprites;
    std::vector<CellDraw> cells;
    std::vector<LightPush> lights;
};

}
