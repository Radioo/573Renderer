#pragma once

#include "formats/gcanim.h"
#include "preset/doc/preset_document.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Preset::Eval {

using Vec2f = std::array<float, 2>;
using Vec3f = std::array<float, 3>;

struct OrbitState {
    float radius = 0.0F;
    float rate = 0.0F;
    float center_x = 0.0F;
    float center_y = 0.0F;
    float z_start = 0.0F;
    float z_per_frame = 0.0F;
    float z_min = 0.0F;
};

struct PulseState {
    Doc::Grid grid = Doc::Grid::A;
    float scale_odd = 1.0F;
    float scale_even = 1.0F;
    int frames = 8;
};

struct ModelSlot {
    std::string name;
    std::string asset;
    bool visible = false;
    int blend_mode = 0;
    float alpha = 1.0F;
    float anim_speed = 1.0F;
    Vec3f position = {0.0F, 0.0F, 0.0F};
    Vec3f rotation = {0.0F, 0.0F, 0.0F};
    Vec3f scale = {1.0F, 1.0F, 1.0F};
    Vec3f spin_per_frame = {0.0F, 0.0F, 0.0F};
    bool has_orbit = false;
    OrbitState orbit = {};
    float spin_kick = 0.0F;
    float spin_kick_decay = 0.0F;
    bool has_pulse = false;
    PulseState pulse = {};
    int draw_start = -1;
    int motion_start = -1;
};

struct SpriteSlot {
    std::string name;
    std::string asset;
    std::string source;
    bool visible = false;
    bool animated = false;
    float x = 0.0F;
    float y = 0.0F;
    float alpha = 1.0F;
    float scale = 1.0F;
    int blend = 0;
    int priority = 0;
    GcAnim::Timing timing = {};
    std::vector<std::string> hidden_parts;
    float scroll_x = 0.0F;
    float scroll_wrap = 0.0F;
    float scroll_offset = 0.0F;
    float speed = 1.0F;
    int offset = 0;
    bool restart_clock = false;
    int draw_start = -1;
};

struct CameraState {
    Vec3f eye = {0.0F, 0.0F, -1.0F};
    Vec3f at = {0.0F, 0.0F, 0.0F};
    Vec3f up = {0.0F, 1.0F, 0.0F};
    float fov_y = 1.0471976F;
    float near_z = 0.1F;
    float far_z = 500.0F;
    bool aspect_auto = true;
    float aspect_value = 1.0F;
};

struct LightState {
    Vec3f direction = {0.0F, 0.0F, -1.0F};
    Vec3f diffuse = {1.0F, 1.0F, 1.0F};
    Vec3f specular = {1.0F, 1.0F, 1.0F};
    bool enabled = true;
};

struct JitterState {
    bool active = false;
    int span = 0;
    float scale = 0.0F;
    Doc::JitterMode mode = Doc::JitterMode::Set;
    std::vector<std::string> models;
};

struct BeatState {
    int rate = 0;
    int span = 1;
    int offset_a = 0;
    int offset_b = 0;
};

enum class WriteKind : uint8_t {
    Alpha,
    AnimSpeed,
    BlendMode,
    ModelScale,
    CameraView,
    CameraProjection,
};

struct MaterialWrite {
    WriteKind kind = WriteKind::Alpha;
    int model = -1;
    bool legacy = false;
    float scalar = 0.0F;
    int integer = 0;
    Vec3f vector = {0.0F, 0.0F, 0.0F};
    CameraState camera = {};
};

struct FrameState {
    int frame = 0;
    Doc::Shading shading = Doc::Shading::LitMaterial;
    int sprite_split_priority = 30;
    CameraState camera = {};
    std::vector<LightState> lights;
    std::vector<ModelSlot> models;
    std::vector<SpriteSlot> sprites;
    JitterState jitter = {};
    BeatState beat = {};
    std::vector<const Doc::Clip*> emitters;
    std::vector<const Doc::Clip*> seeds;
    std::vector<MaterialWrite> writes;
    bool boundary = false;
};

}
