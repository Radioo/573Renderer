#pragma once

#include "formats/gcanim.h"
#include "preset/doc/preset_enum_names.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Preset::Doc {

using Vec2 = std::array<double, 2>;
using Vec3 = std::array<double, 3>;

using OverrideValue = std::variant<bool, double, std::string, Vec3>;

struct Orbit {
    double radius = 0.0;
    double rate_rad_per_frame = 0.0;
    Vec2 center = {0.0, 0.0};
    double z_start = 0.0;
    double z_per_frame = 0.0;
    double z_min = 0.0;
    bool operator==(const Orbit&) const = default;
};

struct PulseSpec {
    Grid grid = Grid::A;
    double scale_odd = 1.0;
    double scale_even = 1.0;
    int frames = 8;
    bool operator==(const PulseSpec&) const = default;
};

struct Scatter {
    Vec2 span = {0.0, 0.0};
    Vec2 offset = {0.0, 0.0};
    bool operator==(const Scatter&) const = default;
};

struct ClipTime {
    ClipClock clock = ClipClock::Continue;
    std::optional<int> ticks = {};
    bool operator==(const ClipTime&) const = default;
};

struct AspectSpec {
    bool automatic = true;
    double value = 0.0;
    bool operator==(const AspectSpec&) const = default;
};

struct SpriteDraw {
    std::string asset = {};
    std::string cell = {};
    double x = 0.0;
    double y = 0.0;
    double alpha = 1.0;
    double scale = 1.0;
    SpriteBlend blend = SpriteBlend::Normal;
    int priority = 0;
    bool operator==(const SpriteDraw&) const = default;
};

struct SpriteAnimate {
    std::string asset = {};
    std::string animation = {};
    double x = 0.0;
    double y = 0.0;
    double alpha = 1.0;
    double scale = 1.0;
    int priority = 0;
    GcAnim::Playback playback = GcAnim::Playback::Loop;
    int loop_start = 0;
    int loop_end = 0;
    int offset = 0;
    std::optional<ClipClock> clock = {};
    double speed = 1.0;
    std::vector<std::string> hidden_parts = {};
    bool operator==(const SpriteAnimate&) const = default;
};

struct SpriteScroll {
    double scroll_x = 0.0;
    double scroll_wrap = 0.0;
    double scroll_offset = 0.0;
    bool operator==(const SpriteScroll&) const = default;
};

struct EmitterCmd {
    std::string asset = {};
    std::string cell = {};
    Spawn spawn = Spawn::ClipStart;
    int count = 0;
    double angle_step_deg = 0.0;
    double phase_rate_deg = 0.0;
    double phase_amplitude_deg = 0.0;
    int radius_from = 0;
    int radius_to = 0;
    int reach_frames = 0;
    std::optional<Vec2> center = {};
    int priority = 0;
    SpriteBlend blend = SpriteBlend::Additive;
    int scale_percent = 100;
    std::optional<Scatter> scatter = {};
    Grid beat_grid = Grid::A;
    int beat_odd = 0;
    int life = 0;
    int life_base = 0;
    int life_span = 0;
    bool operator==(const EmitterCmd&) const = default;
};

struct ModelDraw {
    std::string asset = {};
    std::string model = {};
    ModelBlend blend_mode = ModelBlend::Opaque;
    double alpha = 1.0;
    double anim_speed = 1.0;
    Vec3 position = {0.0, 0.0, 0.0};
    Vec3 rotation = {0.0, 0.0, 0.0};
    Vec3 scale = {1.0, 1.0, 1.0};
    Vec3 spin_per_frame = {0.0, 0.0, 0.0};
    ClipTime clip_time = {};
    bool operator==(const ModelDraw&) const = default;
};

struct ModelTween {
    bool operator==(const ModelTween&) const = default;
};

struct ModelMotionCmd {
    std::optional<Orbit> orbit = {};
    double spin_kick = 0.0;
    double spin_kick_decay = 0.0;
    std::optional<PulseSpec> pulse = {};
    bool operator==(const ModelMotionCmd&) const = default;
};

struct CameraSet {
    std::optional<Vec3> eye = {};
    std::optional<Vec3> at = {};
    std::optional<Vec3> up = {};
    std::optional<double> fov_y = {};
    std::optional<double> near_z = {};
    std::optional<double> far_z = {};
    std::optional<AspectSpec> aspect = {};
    bool operator==(const CameraSet&) const = default;
};

struct CameraTween {
    bool operator==(const CameraTween&) const = default;
};

struct LightSet {
    int index = 0;
    std::optional<Vec3> direction = {};
    std::optional<Vec3> diffuse = {};
    std::optional<Vec3> specular = {};
    bool enabled = true;
    bool operator==(const LightSet&) const = default;
};

struct ParamOverrideCmd {
    std::string id = {};
    OverrideValue value = {};
    bool operator==(const ParamOverrideCmd&) const = default;
};

struct RenderSettingsCmd {
    std::optional<Shading> shading = {};
    std::optional<int> sprite_split_priority = {};
    bool operator==(const RenderSettingsCmd&) const = default;
};

struct RngSeed {
    int seed = 1;
    bool operator==(const RngSeed&) const = default;
};

struct RhythmBeat {
    int rate = 0;
    int span = 1;
    int offset_a = 0;
    int offset_b = 0;
    bool operator==(const RhythmBeat&) const = default;
};

struct RhythmJitter {
    int span = 0;
    double scale = 0.0;
    std::vector<std::string> models = {};
    JitterMode mode = JitterMode::Set;
    bool operator==(const RhythmJitter&) const = default;
};

struct OptionSelect {
    std::string option = {};
    std::string choice = {};
    bool operator==(const OptionSelect&) const = default;
};

using Command =
    std::variant<SpriteDraw, SpriteAnimate, SpriteScroll, EmitterCmd, ModelDraw, ModelTween,
                 ModelMotionCmd, CameraSet, CameraTween, LightSet, ParamOverrideCmd,
                 RenderSettingsCmd, RngSeed, RhythmBeat, RhythmJitter, OptionSelect>;

using ParamValue =
    std::variant<std::monostate, bool, int, double, std::string, Vec2, Vec3,
                 std::vector<std::string>, AspectSpec, std::optional<ClipClock>, ClipTime,
                 std::optional<Orbit>, std::optional<PulseSpec>, std::optional<Scatter>>;

enum class Family : uint8_t {
    None,
    Sprite,
    ModelDraw,
    Camera,
    Light,
    RenderSettings,
    RhythmBeat,
    RngSeed,
};

struct CommandTraits {
    TrackKind kind = TrackKind::Scene;
    Family family = Family::None;
    bool event = false;
};

inline constexpr std::array<CommandTraits, 16> kCommandTraits = {
    CommandTraits{TrackKind::Sprite, Family::Sprite, false},
    CommandTraits{TrackKind::Sprite, Family::Sprite, false},
    CommandTraits{TrackKind::Sprite, Family::None, false},
    CommandTraits{TrackKind::Fx, Family::None, false},
    CommandTraits{TrackKind::Model, Family::ModelDraw, false},
    CommandTraits{TrackKind::Model, Family::None, false},
    CommandTraits{TrackKind::Model, Family::None, false},
    CommandTraits{TrackKind::Camera, Family::Camera, false},
    CommandTraits{TrackKind::Camera, Family::None, false},
    CommandTraits{TrackKind::Light, Family::Light, false},
    CommandTraits{TrackKind::Scene, Family::None, false},
    CommandTraits{TrackKind::Scene, Family::RenderSettings, false},
    CommandTraits{TrackKind::Scene, Family::RngSeed, true},
    CommandTraits{TrackKind::Scene, Family::RhythmBeat, false},
    CommandTraits{TrackKind::Scene, Family::None, false},
    CommandTraits{TrackKind::Scene, Family::None, true},
};

inline CommandType TypeOf(const Command& command) {
    return (CommandType)command.index();
}

inline const CommandTraits& TraitsFor(CommandType type) {
    return kCommandTraits[(std::size_t)type];
}

inline bool IsEvent(CommandType type) {
    return TraitsFor(type).event;
}

}
