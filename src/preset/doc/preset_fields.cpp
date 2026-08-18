#include "preset/doc/preset_fields.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_enum_names.h"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace Preset::Doc {

namespace {

ParamValue ToParam(bool value) {
    return value;
}
ParamValue ToParam(int value) {
    return value;
}
ParamValue ToParam(double value) {
    return value;
}
ParamValue ToParam(const std::string& value) {
    return value;
}
ParamValue ToParam(const Vec2& value) {
    return value;
}
ParamValue ToParam(const Vec3& value) {
    return value;
}
ParamValue ToParam(const std::vector<std::string>& value) {
    return value;
}
ParamValue ToParam(const AspectSpec& value) {
    return value;
}
ParamValue ToParam(const ClipTime& value) {
    return value;
}
ParamValue ToParam(const std::optional<ClipClock>& value) {
    return value;
}
ParamValue ToParam(const std::optional<Orbit>& value) {
    return value;
}
ParamValue ToParam(const std::optional<PulseSpec>& value) {
    return value;
}
ParamValue ToParam(const std::optional<Scatter>& value) {
    return value;
}
ParamValue ToParam(const OverrideValue& value) {
    return std::visit([](const auto& held) { return ToParam(held); }, value);
}

template <class E>
    requires std::is_enum_v<E>
ParamValue ToParam(E value) {
    return (int)value;
}

template <class T> ParamValue ToParam(const std::optional<T>& value) {
    if (!value.has_value()) return {};
    return ToParam(*value);
}

void FromParam(const ParamValue& value, bool& out) {
    if (const auto* held = std::get_if<bool>(&value)) out = *held;
}
void FromParam(const ParamValue& value, int& out) {
    if (const auto* held = std::get_if<int>(&value)) out = *held;
}
void FromParam(const ParamValue& value, double& out) {
    if (const auto* held = std::get_if<double>(&value)) out = *held;
}
void FromParam(const ParamValue& value, std::string& out) {
    if (const auto* held = std::get_if<std::string>(&value)) out = *held;
}
void FromParam(const ParamValue& value, Vec2& out) {
    if (const auto* held = std::get_if<Vec2>(&value)) out = *held;
}
void FromParam(const ParamValue& value, Vec3& out) {
    if (const auto* held = std::get_if<Vec3>(&value)) out = *held;
}
void FromParam(const ParamValue& value, std::vector<std::string>& out) {
    if (const auto* held = std::get_if<std::vector<std::string>>(&value)) out = *held;
}
void FromParam(const ParamValue& value, AspectSpec& out) {
    if (const auto* held = std::get_if<AspectSpec>(&value)) out = *held;
}
void FromParam(const ParamValue& value, ClipTime& out) {
    if (const auto* held = std::get_if<ClipTime>(&value)) out = *held;
}
void FromParam(const ParamValue& value, std::optional<ClipClock>& out) {
    if (const auto* held = std::get_if<std::optional<ClipClock>>(&value)) out = *held;
}
void FromParam(const ParamValue& value, std::optional<Orbit>& out) {
    if (const auto* held = std::get_if<std::optional<Orbit>>(&value)) out = *held;
}
void FromParam(const ParamValue& value, std::optional<PulseSpec>& out) {
    if (const auto* held = std::get_if<std::optional<PulseSpec>>(&value)) out = *held;
}
void FromParam(const ParamValue& value, std::optional<Scatter>& out) {
    if (const auto* held = std::get_if<std::optional<Scatter>>(&value)) out = *held;
}
void FromParam(const ParamValue& value, OverrideValue& out) {
    if (const auto* as_bool = std::get_if<bool>(&value)) out = *as_bool;
    if (const auto* as_double = std::get_if<double>(&value)) out = *as_double;
    if (const auto* as_string = std::get_if<std::string>(&value)) out = *as_string;
    if (const auto* as_vec = std::get_if<Vec3>(&value)) out = *as_vec;
}

template <class E>
    requires std::is_enum_v<E>
void FromParam(const ParamValue& value, E& out) {
    if (const auto* held = std::get_if<int>(&value)) out = (E)*held;
}

template <class T> void FromParam(const ParamValue& value, std::optional<T>& out) {
    if (std::holds_alternative<std::monostate>(value)) {
        out.reset();
        return;
    }
    T held = {};
    FromParam(value, held);
    out = held;
}

template <class C, auto Member> constexpr FieldDesc Field(FieldDesc desc) {
    desc.get = [](const Command& command) -> ParamValue {
        return ToParam(std::get<C>(command).*Member);
    };
    desc.set = [](Command& command, const ParamValue& value) {
        FromParam(value, std::get<C>(command).*Member);
    };
    return desc;
}

constexpr Range kPixels = {.min = -4096.0F, .max = 4096.0F, .step = 1.0F, .soft = true};
constexpr Range kUnitInterval = {.min = 0.0F, .max = 1.0F, .step = 0.005F};
constexpr Range kSpriteScale = {.min = 0.05F, .max = 16.0F, .step = 0.005F, .soft = true};
constexpr Range kPriority = {.min = 0.0F, .max = 64.0F, .step = 1.0F};
constexpr Range kFrameCount = {.min = 0.0F, .max = 36000.0F, .step = 1.0F, .soft = true};
constexpr Range kSpeed = {.min = 0.0F, .max = 8.0F, .step = 0.005F, .soft = true};
constexpr Range kDegrees = {.min = -360.0F, .max = 360.0F, .soft = true};
constexpr Range kRadius = {.min = 0.0F, .max = 8192.0F, .step = 1.0F, .soft = true};
constexpr Range kWorld = {.min = -1000.0F, .max = 1000.0F, .soft = true};
constexpr Range kSpin = {.min = -0.5F, .max = 0.5F, .step = 0.0005F};
constexpr Range kUnitVector = {.min = -1.0F, .max = 1.0F};
constexpr Range kColor = {.min = 0.0F, .max = 4.0F, .soft = true};

constexpr auto kSpriteDrawFields = std::to_array<FieldDesc>({
    Field<SpriteDraw, &SpriteDraw::asset>({.id = "asset",
                                           .kind = FieldKind::String,
                                           .help = "Package2d asset id the cell comes from.",
                                           .required = true}),
    Field<SpriteDraw, &SpriteDraw::cell>({.id = "cell",
                                          .kind = FieldKind::String,
                                          .help = "Cell name inside the package.",
                                          .required = true}),
    Field<SpriteDraw, &SpriteDraw::x>({.id = "x",
                                       .kind = FieldKind::Float,
                                       .range = kPixels,
                                       .unit = "px",
                                       .help = "Left edge on the document canvas.",
                                       .tweenable = true}),
    Field<SpriteDraw, &SpriteDraw::y>({.id = "y",
                                       .kind = FieldKind::Float,
                                       .range = kPixels,
                                       .unit = "px",
                                       .help = "Top edge on the document canvas.",
                                       .tweenable = true}),
    Field<SpriteDraw, &SpriteDraw::alpha>({.id = "alpha",
                                           .kind = FieldKind::Float,
                                           .range = kUnitInterval,
                                           .help = "Vertex diffuse alpha.",
                                           .tweenable = true}),
    Field<SpriteDraw, &SpriteDraw::scale>({.id = "scale",
                                           .kind = FieldKind::Float,
                                           .range = kSpriteScale,
                                           .help = "Uniform scale about the layer pivot.",
                                           .tweenable = true}),
    Field<SpriteDraw, &SpriteDraw::blend>({.id = "blend",
                                           .kind = FieldKind::Enum,
                                           .help = "Blend mode of the placed cell.",
                                           .enum_names = kSpriteBlendNames}),
    Field<SpriteDraw, &SpriteDraw::priority>({.id = "priority",
                                              .kind = FieldKind::Int,
                                              .range = kPriority,
                                              .help = "Draw order and side of the 3D split."}),
});

constexpr auto kSpriteAnimateFields = std::to_array<FieldDesc>({
    Field<SpriteAnimate, &SpriteAnimate::asset>({.id = "asset",
                                                 .kind = FieldKind::String,
                                                 .help = "Package2d asset id.",
                                                 .required = true}),
    Field<SpriteAnimate, &SpriteAnimate::animation>({.id = "animation",
                                                     .kind = FieldKind::String,
                                                     .help = "Animation name in the package.",
                                                     .required = true}),
    Field<SpriteAnimate, &SpriteAnimate::x>({.id = "x",
                                             .kind = FieldKind::Float,
                                             .range = kPixels,
                                             .unit = "px",
                                             .help = "Left edge on the document canvas.",
                                             .tweenable = true}),
    Field<SpriteAnimate, &SpriteAnimate::y>({.id = "y",
                                             .kind = FieldKind::Float,
                                             .range = kPixels,
                                             .unit = "px",
                                             .help = "Top edge on the document canvas.",
                                             .tweenable = true}),
    Field<SpriteAnimate, &SpriteAnimate::alpha>({.id = "alpha",
                                                 .kind = FieldKind::Float,
                                                 .range = kUnitInterval,
                                                 .help = "Vertex diffuse alpha.",
                                                 .tweenable = true}),
    Field<SpriteAnimate, &SpriteAnimate::scale>({.id = "scale",
                                                 .kind = FieldKind::Float,
                                                 .range = kSpriteScale,
                                                 .help = "Uniform scale about the layer pivot.",
                                                 .tweenable = true}),
    Field<SpriteAnimate, &SpriteAnimate::priority>(
        {.id = "priority",
         .kind = FieldKind::Int,
         .range = kPriority,
         .help = "Draw order and side of the 3D split."}),
    Field<SpriteAnimate, &SpriteAnimate::playback>({.id = "playback",
                                                    .kind = FieldKind::Enum,
                                                    .help = "What the animation does at its end.",
                                                    .enum_names = kPlaybackNames}),
    Field<SpriteAnimate, &SpriteAnimate::loop_start>({.id = "loop_start",
                                                      .kind = FieldKind::Int,
                                                      .range = kFrameCount,
                                                      .unit = "frames",
                                                      .help = "First frame of the loop range."}),
    Field<SpriteAnimate, &SpriteAnimate::loop_end>({.id = "loop_end",
                                                    .kind = FieldKind::Int,
                                                    .range = kFrameCount,
                                                    .unit = "frames",
                                                    .help = "Last frame of the loop range."}),
    Field<SpriteAnimate, &SpriteAnimate::offset>(
        {.id = "offset",
         .kind = FieldKind::Int,
         .range = kFrameCount,
         .unit = "frames",
         .help = "Animation frame at the clip start when the clock restarts."}),
    Field<SpriteAnimate, &SpriteAnimate::clock>(
        {.id = "clock",
         .kind = FieldKind::Clock,
         .help = "Null auto, continue keeps the instance clock, restart sets it to offset."}),
    Field<SpriteAnimate, &SpriteAnimate::speed>({.id = "speed",
                                                 .kind = FieldKind::Float,
                                                 .range = kSpeed,
                                                 .unit = "frames/frame",
                                                 .help = "Animation frames per document frame.",
                                                 .tweenable = true}),
    Field<SpriteAnimate, &SpriteAnimate::hidden_parts>(
        {.id = "hidden_parts",
         .kind = FieldKind::StringList,
         .help = "Child animations or cells the placement skips."}),
});

constexpr auto kSpriteScrollFields = std::to_array<FieldDesc>({
    Field<SpriteScroll, &SpriteScroll::scroll_x>({.id = "scroll_x",
                                                  .kind = FieldKind::Float,
                                                  .range = {.min = -64.0F, .max = 64.0F},
                                                  .unit = "px/frame",
                                                  .help = "Horizontal wrap scroll speed.",
                                                  .tweenable = true}),
    Field<SpriteScroll, &SpriteScroll::scroll_wrap>({.id = "scroll_wrap",
                                                     .kind = FieldKind::Float,
                                                     .range = kRadius,
                                                     .unit = "px",
                                                     .help = "Width the scroll wraps at."}),
    Field<SpriteScroll, &SpriteScroll::scroll_offset>(
        {.id = "scroll_offset",
         .kind = FieldKind::Float,
         .range = kRadius,
         .unit = "px",
         .help = "Scroll position when the instance appears with its clock at zero."}),
});

constexpr auto kEmitterFields = std::to_array<FieldDesc>({
    Field<EmitterCmd, &EmitterCmd::asset>({.id = "asset",
                                           .kind = FieldKind::String,
                                           .help = "Package2d asset id of the particle cell.",
                                           .required = true}),
    Field<EmitterCmd, &EmitterCmd::cell>(
        {.id = "cell", .kind = FieldKind::String, .help = "Particle cell name.", .required = true}),
    Field<EmitterCmd, &EmitterCmd::spawn>({.id = "spawn",
                                           .kind = FieldKind::Enum,
                                           .help = "When the emitter spawns particles.",
                                           .enum_names = kSpawnNames}),
    Field<EmitterCmd, &EmitterCmd::count>({.id = "count",
                                           .kind = FieldKind::Int,
                                           .range = kFrameCount,
                                           .help = "Particles per spawn."}),
    Field<EmitterCmd, &EmitterCmd::angle_step_deg>({.id = "angle_step_deg",
                                                    .kind = FieldKind::Float,
                                                    .range = kDegrees,
                                                    .unit = "deg",
                                                    .help = "Ring angle step per particle."}),
    Field<EmitterCmd, &EmitterCmd::phase_rate_deg>({.id = "phase_rate_deg",
                                                    .kind = FieldKind::Float,
                                                    .range = kDegrees,
                                                    .unit = "deg",
                                                    .help = "Ring phase rate per absolute frame."}),
    Field<EmitterCmd, &EmitterCmd::phase_amplitude_deg>({.id = "phase_amplitude_deg",
                                                         .kind = FieldKind::Float,
                                                         .range = kDegrees,
                                                         .unit = "deg",
                                                         .help = "Ring phase amplitude."}),
    Field<EmitterCmd, &EmitterCmd::radius_from>({.id = "radius_from",
                                                 .kind = FieldKind::Int,
                                                 .range = kRadius,
                                                 .unit = "px",
                                                 .help = "Ring reach at the clip start."}),
    Field<EmitterCmd, &EmitterCmd::radius_to>({.id = "radius_to",
                                               .kind = FieldKind::Int,
                                               .range = kRadius,
                                               .unit = "px",
                                               .help = "Ring reach after reach_frames."}),
    Field<EmitterCmd, &EmitterCmd::reach_frames>({.id = "reach_frames",
                                                  .kind = FieldKind::Int,
                                                  .range = kFrameCount,
                                                  .unit = "frames",
                                                  .help = "Frames the ring reach lerps over."}),
    Field<EmitterCmd, &EmitterCmd::center>({.id = "center",
                                            .kind = FieldKind::Vec2,
                                            .range = kPixels,
                                            .unit = "px",
                                            .help = "Ring centre, canvas centre when absent.",
                                            .tweenable = true}),
    Field<EmitterCmd, &EmitterCmd::priority>({.id = "priority",
                                              .kind = FieldKind::Int,
                                              .range = kPriority,
                                              .help = "Draw order and side of the 3D split."}),
    Field<EmitterCmd, &EmitterCmd::blend>({.id = "blend",
                                           .kind = FieldKind::Enum,
                                           .help = "Blend mode of every particle.",
                                           .enum_names = kSpriteBlendNames}),
    Field<EmitterCmd, &EmitterCmd::scale_percent>({.id = "scale_percent",
                                                   .kind = FieldKind::Int,
                                                   .range = kRadius,
                                                   .unit = "percent",
                                                   .help = "Particle scale in percent."}),
    Field<EmitterCmd, &EmitterCmd::scatter>({.id = "scatter",
                                             .kind = FieldKind::ScatterField,
                                             .help = "Random target box, null for the ring."}),
    Field<EmitterCmd, &EmitterCmd::beat_grid>({.id = "beat_grid",
                                               .kind = FieldKind::Enum,
                                               .help = "Beat grid a spawn rule reads.",
                                               .enum_names = kGridNames}),
    Field<EmitterCmd, &EmitterCmd::beat_odd>({.id = "beat_odd",
                                              .kind = FieldKind::Int,
                                              .range = {.min = 0.0F, .max = 1.0F, .step = 1.0F},
                                              .help = "Beat index parity a beat spawn fires on."}),
    Field<EmitterCmd, &EmitterCmd::life>({.id = "life",
                                          .kind = FieldKind::Int,
                                          .range = kFrameCount,
                                          .unit = "frames",
                                          .help = "Fixed particle life."}),
    Field<EmitterCmd, &EmitterCmd::life_base>({.id = "life_base",
                                               .kind = FieldKind::Int,
                                               .range = kFrameCount,
                                               .unit = "frames",
                                               .help = "Lowest life of a random life."}),
    Field<EmitterCmd, &EmitterCmd::life_span>({.id = "life_span",
                                               .kind = FieldKind::Int,
                                               .range = kFrameCount,
                                               .unit = "frames",
                                               .help = "Random life spread over life_base."}),
});

constexpr auto kModelDrawFields = std::to_array<FieldDesc>({
    Field<ModelDraw, &ModelDraw::asset>({.id = "asset",
                                         .kind = FieldKind::String,
                                         .help = "Scene3d asset id the model comes from.",
                                         .required = true}),
    Field<ModelDraw, &ModelDraw::model>({.id = "model",
                                         .kind = FieldKind::String,
                                         .help = "Model name, the track target when empty."}),
    Field<ModelDraw, &ModelDraw::blend_mode>({.id = "blend_mode",
                                              .kind = FieldKind::Enum,
                                              .help = "Render blend mode of the model.",
                                              .tweenable = true,
                                              .enum_names = kModelBlendNames}),
    Field<ModelDraw, &ModelDraw::alpha>({.id = "alpha",
                                         .kind = FieldKind::Float,
                                         .range = kUnitInterval,
                                         .help = "Model alpha.",
                                         .tweenable = true}),
    Field<ModelDraw, &ModelDraw::anim_speed>(
        {.id = "anim_speed",
         .kind = FieldKind::Float,
         .range = kSpeed,
         .unit = "ticks/frame",
         .help = "3D ticks the model clock advances per frame.",
         .tweenable = true}),
    Field<ModelDraw, &ModelDraw::position>({.id = "position",
                                            .kind = FieldKind::Vec3,
                                            .range = kWorld,
                                            .unit = "world",
                                            .help = "Model position.",
                                            .tweenable = true}),
    Field<ModelDraw, &ModelDraw::rotation>({.id = "rotation",
                                            .kind = FieldKind::Vec3,
                                            .range = {.min = -50.0F, .max = 50.0F, .soft = true},
                                            .unit = "rad",
                                            .help = "Base rotation before the accumulated spin.",
                                            .tweenable = true}),
    Field<ModelDraw, &ModelDraw::scale>({.id = "scale",
                                         .kind = FieldKind::Vec3,
                                         .range = {.min = 0.001F, .max = 100.0F, .soft = true},
                                         .unit = "x",
                                         .help = "Model scale.",
                                         .tweenable = true}),
    Field<ModelDraw, &ModelDraw::spin_per_frame>({.id = "spin_per_frame",
                                                  .kind = FieldKind::Vec3,
                                                  .range = kSpin,
                                                  .unit = "rad/frame",
                                                  .help = "Rate the per model spin integrates.",
                                                  .tweenable = true}),
    Field<ModelDraw, &ModelDraw::clip_time>({.id = "clip_time",
                                             .kind = FieldKind::ClipTimeField,
                                             .unit = "ticks",
                                             .help = "3D clip time at the clip start."}),
});

constexpr auto kModelMotionFields = std::to_array<FieldDesc>({
    Field<ModelMotionCmd, &ModelMotionCmd::orbit>({.id = "orbit",
                                                   .kind = FieldKind::OrbitField,
                                                   .help = "Orbit path, null when the model does "
                                                           "not orbit."}),
    Field<ModelMotionCmd, &ModelMotionCmd::spin_kick>(
        {.id = "spin_kick",
         .kind = FieldKind::Float,
         .range = {.min = 0.0F, .max = 64.0F, .step = 0.05F},
         .help = "Spin multiplier armed once at the clip start."}),
    Field<ModelMotionCmd, &ModelMotionCmd::spin_kick_decay>(
        {.id = "spin_kick_decay",
         .kind = FieldKind::Float,
         .range = {.min = 0.0F, .max = 8.0F, .step = 0.005F},
         .help = "Amount the kick multiplier moves back toward one per frame."}),
    Field<ModelMotionCmd, &ModelMotionCmd::pulse>({.id = "pulse",
                                                   .kind = FieldKind::PulseField,
                                                   .help = "Beat driven scale pulse, null for "
                                                           "none."}),
});

constexpr auto kCameraSetFields = std::to_array<FieldDesc>({
    Field<CameraSet, &CameraSet::eye>({.id = "eye",
                                       .kind = FieldKind::Vec3,
                                       .range = kWorld,
                                       .unit = "world",
                                       .help = "Camera position.",
                                       .tweenable = true}),
    Field<CameraSet, &CameraSet::at>({.id = "at",
                                      .kind = FieldKind::Vec3,
                                      .range = kWorld,
                                      .unit = "world",
                                      .help = "Point the camera faces.",
                                      .tweenable = true}),
    Field<CameraSet, &CameraSet::up>({.id = "up",
                                      .kind = FieldKind::Vec3,
                                      .range = {.min = -1.0F, .max = 1.0F, .soft = true},
                                      .unit = "world",
                                      .help = "Camera up vector.",
                                      .tweenable = true}),
    Field<CameraSet, &CameraSet::fov_y>({.id = "fov_y",
                                         .kind = FieldKind::Float,
                                         .range = {.min = 0.0F, .max = 64.0F, .soft = true},
                                         .unit = "rad",
                                         .help = "Vertical field of view.",
                                         .tweenable = true}),
    Field<CameraSet, &CameraSet::near_z>(
        {.id = "near_z",
         .kind = FieldKind::Float,
         .range = {.min = 0.0F, .max = 100.0F, .step = 0.001F, .soft = true},
         .help = "Near plane.",
         .tweenable = true}),
    Field<CameraSet, &CameraSet::far_z>({.id = "far_z",
                                         .kind = FieldKind::Float,
                                         .range = {.min = 0.1F, .max = 10000.0F, .soft = true},
                                         .help = "Far plane.",
                                         .tweenable = true}),
    Field<CameraSet, &CameraSet::aspect>({.id = "aspect",
                                          .kind = FieldKind::Aspect,
                                          .help = "Positive number or auto from the render size."}),
});

constexpr auto kLightSetFields = std::to_array<FieldDesc>({
    Field<LightSet, &LightSet::index>(
        {.id = "index",
         .kind = FieldKind::Int,
         .range = {.min = 0.0F, .max = 8.0F, .step = 1.0F, .soft = true},
         .help = "Light slot the clip drives."}),
    Field<LightSet, &LightSet::direction>({.id = "direction",
                                           .kind = FieldKind::Vec3,
                                           .range = kUnitVector,
                                           .unit = "world",
                                           .help = "Directional light direction.",
                                           .tweenable = true}),
    Field<LightSet, &LightSet::diffuse>({.id = "diffuse",
                                         .kind = FieldKind::Vec3,
                                         .range = kColor,
                                         .unit = "rgb",
                                         .help = "Diffuse colour.",
                                         .tweenable = true}),
    Field<LightSet, &LightSet::specular>({.id = "specular",
                                          .kind = FieldKind::Vec3,
                                          .range = kColor,
                                          .unit = "rgb",
                                          .help = "Specular colour.",
                                          .tweenable = true}),
    Field<LightSet, &LightSet::enabled>(
        {.id = "enabled", .kind = FieldKind::Bool, .help = "Whether the slot is lit."}),
});

constexpr auto kParamOverrideFields = std::to_array<FieldDesc>({
    Field<ParamOverrideCmd, &ParamOverrideCmd::id>({.id = "id",
                                                    .kind = FieldKind::String,
                                                    .help = "Schema id the override writes.",
                                                    .required = true}),
    Field<ParamOverrideCmd, &ParamOverrideCmd::value>({.id = "value",
                                                       .kind = FieldKind::OverrideField,
                                                       .help = "Value typed by the schema kind."}),
});

constexpr auto kRenderSettingsFields = std::to_array<FieldDesc>({
    Field<RenderSettingsCmd, &RenderSettingsCmd::shading>({.id = "shading",
                                                           .kind = FieldKind::Enum,
                                                           .help = "Shading override.",
                                                           .enum_names = kShadingNames}),
    Field<RenderSettingsCmd, &RenderSettingsCmd::sprite_split_priority>(
        {.id = "sprite_split_priority",
         .kind = FieldKind::Int,
         .range = kPriority,
         .help = "Priority at which 2D layers move behind the 3D."}),
});

constexpr auto kRngSeedFields = std::to_array<FieldDesc>({
    Field<RngSeed, &RngSeed::seed>(
        {.id = "seed",
         .kind = FieldKind::Int,
         .range = {.min = 0.0F, .max = 2000000000.0F, .step = 1.0F, .soft = true},
         .help = "Value the shared Ran3 stream is reseeded with."}),
});

constexpr auto kRhythmBeatFields = std::to_array<FieldDesc>({
    Field<RhythmBeat, &RhythmBeat::rate>({.id = "rate",
                                          .kind = FieldKind::Int,
                                          .range = {.min = 0.0F, .max = 1000.0F, .step = 1.0F},
                                          .help = "Beat numerator."}),
    Field<RhythmBeat, &RhythmBeat::span>({.id = "span",
                                          .kind = FieldKind::Int,
                                          .range = {.min = 1.0F, .max = 36000.0F, .step = 1.0F},
                                          .unit = "frames",
                                          .help = "Frames one beat period covers."}),
    Field<RhythmBeat, &RhythmBeat::offset_a>(
        {.id = "offset_a",
         .kind = FieldKind::Int,
         .range = {.min = -3600.0F, .max = 3600.0F, .step = 1.0F},
         .unit = "frames",
         .help = "Frame grid A counts from."}),
    Field<RhythmBeat, &RhythmBeat::offset_b>(
        {.id = "offset_b",
         .kind = FieldKind::Int,
         .range = {.min = -3600.0F, .max = 3600.0F, .step = 1.0F},
         .unit = "frames",
         .help = "Frame grid B counts from."}),
});

constexpr auto kRhythmJitterFields = std::to_array<FieldDesc>({
    Field<RhythmJitter, &RhythmJitter::span>({.id = "span",
                                              .kind = FieldKind::Int,
                                              .range = {.min = 0.0F, .max = 4096.0F, .step = 1.0F},
                                              .help = "Width of the random draw."}),
    Field<RhythmJitter, &RhythmJitter::scale>(
        {.id = "scale",
         .kind = FieldKind::Float,
         .range = {.min = 0.0F, .max = 0.01F, .step = 0.000001F},
         .help = "World units one random step is worth."}),
    Field<RhythmJitter, &RhythmJitter::models>({.id = "models",
                                                .kind = FieldKind::StringList,
                                                .help = "Models to shake, empty for every model."}),
    Field<RhythmJitter, &RhythmJitter::mode>({.id = "mode",
                                              .kind = FieldKind::Enum,
                                              .help = "Set replaces the position, add offsets it.",
                                              .enum_names = kJitterModeNames}),
});

constexpr auto kOptionSelectFields = std::to_array<FieldDesc>({
    Field<OptionSelect, &OptionSelect::option>({.id = "option",
                                                .kind = FieldKind::String,
                                                .help = "Option the event switches.",
                                                .required = true}),
    Field<OptionSelect, &OptionSelect::choice>({.id = "choice",
                                                .kind = FieldKind::String,
                                                .help = "Choice label the event selects.",
                                                .required = true}),
});

constexpr std::array<FieldDesc, 0> kNoFields = {};

constexpr std::array<std::span<const FieldDesc>, 16> kTables = {
    kSpriteDrawFields,   kSpriteAnimateFields, kSpriteScrollFields,
    kEmitterFields,      kModelDrawFields,     kNoFields,
    kModelMotionFields,  kCameraSetFields,     kNoFields,
    kLightSetFields,     kParamOverrideFields, kRenderSettingsFields,
    kRngSeedFields,      kRhythmBeatFields,    kRhythmJitterFields,
    kOptionSelectFields,
};

static_assert(kTables.size() == std::variant_size_v<Command>);
static_assert(kCommandTypeNames.size() == std::variant_size_v<Command>);
static_assert(kCommandTraits.size() == std::variant_size_v<Command>);

}

std::span<const FieldDesc> FieldsFor(CommandType type) {
    return kTables[(std::size_t)type];
}

std::span<const FieldDesc> KeyFieldsFor(CommandType type) {
    if (type == CommandType::ModelTween) return FieldsFor(CommandType::ModelDraw);
    if (type == CommandType::CameraTween) return FieldsFor(CommandType::CameraSet);
    return FieldsFor(type);
}

const FieldDesc* FindField(std::span<const FieldDesc> fields, std::string_view id) {
    for (const FieldDesc& field : fields) {
        if (field.id == id) return &field;
    }
    return nullptr;
}

const Command& DefaultCommand(CommandType type) {
    static const std::array<Command, 16> defaults = {
        Command{SpriteDraw{}},     Command{SpriteAnimate{}},    Command{SpriteScroll{}},
        Command{EmitterCmd{}},     Command{ModelDraw{}},        Command{ModelTween{}},
        Command{ModelMotionCmd{}}, Command{CameraSet{}},        Command{CameraTween{}},
        Command{LightSet{}},       Command{ParamOverrideCmd{}}, Command{RenderSettingsCmd{}},
        Command{RngSeed{}},        Command{RhythmBeat{}},       Command{RhythmJitter{}},
        Command{OptionSelect{}},
    };
    return defaults[(std::size_t)type];
}

}
