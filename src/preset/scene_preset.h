#pragma once

#include "formats/gcanim.h"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace Preset {

struct ModelMotion {
    float orbit_radius = 0.0F;
    float orbit_rate = 0.0F;
    float center_x = 0.0F;
    float center_y = 0.0F;
    float z_start = 0.0F;
    float z_per_frame = 0.0F;
    float z_min = 0.0F;
    std::array<float, 3> spin_per_frame = {0.0F, 0.0F, 0.0F};
    float spin_kick = 0.0F;
    float spin_kick_decay = 0.0F;
};

struct ParamOverride {
    std::string_view id;
    std::array<float, 3> f = {0.0F, 0.0F, 0.0F};
    int i = 0;
};

struct OptionChoice {
    std::string_view label;
    std::array<float, 3> position = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> camera_eye = {0.0F, 0.0F, 0.0F};
    bool moves_camera = false;
    std::span<const ParamOverride> params = {};
};

struct Option {
    std::string_view id;
    std::string_view label;
    std::span<const OptionChoice> choices = {};
    int default_choice = 0;
    int transition_frames = 0;
    float spin_kick = 0.0F;
};

struct ModelLayer {
    std::string_view scene_dir;
    std::string_view model;
    int blend_mode = 0;
    float alpha = 1.0F;
    float anim_speed = 1.0F;
    std::array<float, 3> position = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> rotation = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> scale = {1.0F, 1.0F, 1.0F};
    ModelMotion motion = {};
};

struct SpriteLayer {
    std::string_view package_dir;
    std::string_view sprite;
    bool animated = false;
    float x = 0.0F;
    float y = 0.0F;
    float alpha = 1.0F;
    float scale = 1.0F;
    int blend = 0;
    int priority = 0;
    GcAnim::Timing timing = {};
    std::span<const std::string_view> hidden_parts = {};
    float scroll_x = 0.0F;
    float scroll_wrap = 0.0F;
};

struct DirectionalLight {
    std::array<float, 3> direction = {0.0F, 0.0F, -1.0F};
    std::array<float, 3> diffuse = {1.0F, 1.0F, 1.0F};
    std::array<float, 3> specular = {1.0F, 1.0F, 1.0F};
};

struct Camera {
    std::array<float, 3> eye = {0.0F, 0.0F, -1.0F};
    std::array<float, 3> at = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> up = {0.0F, 1.0F, 0.0F};
    float fov_y = 1.0471976F;
    float near_z = 0.1F;
    float far_z = 500.0F;
    float aspect = 0.0F;
};

struct Countdown {
    int start_frames = 0;
    int ramp_below = 0;
    float speed_base = 1.0F;
    float speed_per_frame = 0.0F;
    float fade_from = 1.0F;
    float fade_per_frame = 0.0F;
    int ramp_blend_mode = 0;
};

struct Intro {
    int frames = 0;
    float speed_from = 0.0F;
    float speed_to = 0.0F;
    float fov_from = 0.0F;
    float fov_to = 0.0F;
};

enum class Curve : uint8_t {
    Linear,
    Sine,
};

struct Ramp {
    std::string_view id;
    std::array<float, 3> from = {0.0F, 0.0F, 0.0F};
    std::array<float, 3> to = {0.0F, 0.0F, 0.0F};
    int frames = 0;
    float degrees_per_frame = 0.0F;
    Curve curve = Curve::Linear;
};

enum class Grid : uint8_t {
    A,
    B,
};

enum class Spawn : uint8_t {
    PhaseStart,
    EveryFrame,
    Beat,
};

struct Beat {
    int rate = 0;
    int span = 1;
    int offset_a = 0;
    int offset_b = 0;
};

struct Pulse {
    Grid grid = Grid::A;
    float scale_odd = 1.0F;
    float scale_even = 1.0F;
    int frames = 8;
};

struct Jitter {
    int from_frame = 0;
    int span = 0;
    float scale = 0.0F;
};

struct Emitter {
    std::string_view package_dir;
    std::string_view cell;
    int count = 0;
    float angle_step_deg = 0.0F;
    float phase_rate_deg = 0.0F;
    float phase_amplitude_deg = 0.0F;
    int radius_from = 0;
    int radius_to = 0;
    int frames = 0;
    float center_x = 320.0F;
    float center_y = 240.0F;
    int priority = 0;
    int blend = 1;
    int scale = 100;
    bool scatter = false;
    int span_x = 0;
    int span_y = 0;
    int offset_x = 0;
    int offset_y = 0;
    Spawn spawn = Spawn::PhaseStart;
    Grid beat_grid = Grid::A;
    int beat_odd = 0;
    int life = 0;
    int life_base = 0;
    int life_span = 0;
};

struct Phase {
    std::string_view label;
    int start_frame = 0;
    std::span<const ParamOverride> params = {};
    std::span<const Ramp> ramps = {};
    std::span<const Emitter> emitters = {};
};

enum class Shading : uint8_t {
    TextureOnly,
    LitMaterial,
};

struct Scene {
    std::string_view id;
    std::string_view name;
    std::string_view build;
    int render_w = 640;
    int render_h = 480;
    Shading shading = Shading::TextureOnly;
    int sprite_split_priority = 30;
    Camera camera = {};
    Countdown countdown = {};
    Intro intro = {};
    Beat beat = {};
    Pulse pulse = {};
    Jitter jitter = {};
    int rng_seed = 1;
    bool opaque_screen = true;
    std::span<const ModelLayer> models;
    std::span<const SpriteLayer> sprites;
    std::span<const DirectionalLight> lights;
    std::span<const Option> options = {};
    std::span<const Phase> phases = {};
};

std::vector<const Scene*> ForBuild(std::string_view build);

}
