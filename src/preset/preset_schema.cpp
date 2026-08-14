#include "formats/gcanim.h"
#include "preset/preset_effective.h"
#include "preset/preset_params.h"

#include "preset/scene_preset.h"
#include <array>
#include <span>
#include <string_view>

namespace Preset {

template <> struct ScopeOwner<Scope::Scene> {
    using type = Effective;
};
template <> struct ScopeOwner<Scope::Camera> {
    using type = Camera;
};
template <> struct ScopeOwner<Scope::Countdown> {
    using type = Countdown;
};
template <> struct ScopeOwner<Scope::Intro> {
    using type = Intro;
};
template <> struct ScopeOwner<Scope::Light> {
    using type = DirectionalLight;
};
template <> struct ScopeOwner<Scope::Model> {
    using type = ModelState;
};
template <> struct ScopeOwner<Scope::ModelMotion> {
    using type = ModelMotion;
};
template <> struct ScopeOwner<Scope::Sprite> {
    using type = SpriteState;
};
template <> struct ScopeOwner<Scope::SpriteTiming> {
    using type = GcAnim::Timing;
};
template <> struct ScopeOwner<Scope::Option> {
    using type = OptionState;
};
template <> struct ScopeOwner<Scope::OptionChoice> {
    using type = ChoiceState;
};

namespace {

constexpr std::array<std::string_view, 2> kShadingLabels = {"texture only", "lit material"};
constexpr std::array<std::string_view, 5> kBlendLabels = {"opaque", "opaque (1)", "alpha",
                                                          "additive", "subtract"};
constexpr std::array<std::string_view, 3> kSpriteBlendLabels = {"normal", "additive", "subtract"};
constexpr std::array<std::string_view, 3> kPlaybackLabels = {"loop", "hold last", "hide after end"};

constexpr auto kRows = std::to_array<ParamDesc>({
    Row<Scope::Scene, &Effective::sprite_split_priority>(
        {.key = "sprite_split_priority",
         .label = "2D split priority",
         .group = "Render",
         .kind = ValueKind::Int,
         .range = {.min = 0.0F, .max = 64.0F, .step = 1.0F},
         .apply = Apply::Live,
         .help = "Layers at or above this priority draw behind the 3D models, below it in front.",
         .aliases = "split order depth"}),
    Row<Scope::Scene, &Effective::shading>(
        {.key = "shading",
         .label = "Shading",
         .group = "Render",
         .kind = ValueKind::Enum,
         .apply = Apply::Rebind,
         .help = "Lit material reproduces the decoded per-object render state, texture only is "
                 "the asset browser's unlit choice.",
         .aliases = "light material style",
         .enum_labels = kShadingLabels}),

    Row<Scope::Camera, &Camera::eye>({.key = "camera.eye",
                                      .label = "Eye",
                                      .group = "Camera",
                                      .kind = ValueKind::Vec3,
                                      .range = {.min = -1000.0F, .max = 1000.0F, .soft = true},
                                      .apply = Apply::Rebind,
                                      .help = "Camera position, the first vector the game hands "
                                              "to its LookAt call.",
                                      .aliases = "position view"}),
    Row<Scope::Camera, &Camera::at>({.key = "camera.at",
                                     .label = "Look at",
                                     .group = "Camera",
                                     .kind = ValueKind::Vec3,
                                     .range = {.min = -1000.0F, .max = 1000.0F, .soft = true},
                                     .apply = Apply::Rebind,
                                     .help = "The point the camera faces.",
                                     .aliases = "target view"}),
    Row<Scope::Camera, &Camera::up>({.key = "camera.up",
                                     .label = "Up",
                                     .group = "Camera",
                                     .kind = ValueKind::Vec3,
                                     .range = {.min = -1.0F, .max = 1.0F, .soft = true},
                                     .apply = Apply::Rebind,
                                     .help = "Camera up vector.",
                                     .aliases = "roll orientation"}),
    Row<Scope::Camera, &Camera::fov_y>({.key = "camera.fov_y",
                                        .label = "Field of view",
                                        .group = "Camera",
                                        .kind = ValueKind::Float,
                                        .range = {.min = 0.05F, .max = 3.05F, .step = 0.001F},
                                        .apply = Apply::Rebind,
                                        .unit = "rad",
                                        .help = "Vertical field of view handed to the projection.",
                                        .aliases = "fov perspective zoom"}),
    Row<Scope::Camera, &Camera::near_z>(
        {.key = "camera.near_z",
         .label = "Near plane",
         .group = "Camera",
         .kind = ValueKind::Float,
         .range = {.min = 0.0F, .max = 100.0F, .step = 0.001F, .soft = true},
         .apply = Apply::Rebind,
         .help = "Several IIDX screens author this as 0, which degenerates the depth range. The "
                 "renderer substitutes an epsilon internally and never writes it back.",
         .aliases = "clip znear"}),
    Row<Scope::Camera, &Camera::far_z>({.key = "camera.far_z",
                                        .label = "Far plane",
                                        .group = "Camera",
                                        .kind = ValueKind::Float,
                                        .range = {.min = 0.1F, .max = 10000.0F, .soft = true},
                                        .apply = Apply::Rebind,
                                        .help = "Far clip distance.",
                                        .aliases = "clip zfar"}),
    Row<Scope::Scene, &Effective::aspect_auto>(
        {.key = "camera.aspect_auto",
         .label = "Aspect from viewport",
         .group = "Camera",
         .kind = ValueKind::Bool,
         .apply = Apply::Rebind,
         .help = "On when the screen leaves the projection aspect at its derive-from-viewport "
                 "sentinel of zero.",
         .aliases = "aspect ratio auto"}),
    Row<Scope::Scene, &Effective::aspect_value>(
        {.key = "camera.aspect_value",
         .label = "Aspect",
         .group = "Camera",
         .kind = ValueKind::Float,
         .range = {.min = 0.5F, .max = 3.0F, .step = 0.0001F},
         .apply = Apply::Rebind,
         .help = "Explicit projection aspect. IIDX RED authors 850/480 on a 640x480 target.",
         .aliases = "aspect ratio"}),

    Row<Scope::Countdown, &Countdown::speed_base>(
        {.key = "countdown.speed_base",
         .label = "Ramp speed base",
         .group = "Timing",
         .kind = ValueKind::Float,
         .range = {.min = 0.0F, .max = 8.0F, .step = 0.005F},
         .apply = Apply::Live,
         .unit = "ticks/frame",
         .help = "Animation speed the lead model ramps from once the screen timer crosses its "
                 "threshold.",
         .aliases = "timer ramp speed"}),

    Row<Scope::Model, &ModelState::blend_mode>(
        {.key = "blend_mode",
         .label = "Draw mode",
         .group = "Model",
         .kind = ValueKind::Enum,
         .apply = Apply::Rebind,
         .help = "Modes 0 and 1 draw opaque and unsorted, 2 3 and 4 are depth sorted and drawn "
                 "after, with 3 additive and 4 reverse subtract.",
         .aliases = "blend additive pass",
         .enum_labels = kBlendLabels}),
    Row<Scope::Model, &ModelState::alpha>({.key = "alpha",
                                           .label = "Alpha",
                                           .group = "Model",
                                           .kind = ValueKind::Float,
                                           .range = {.min = 0.0F, .max = 1.0F, .step = 0.005F},
                                           .apply = Apply::Rebind,
                                           .help = "Per-model alpha the screen sets.",
                                           .aliases = "opacity transparency"}),
    Row<Scope::Model, &ModelState::anim_speed>(
        {.key = "anim_speed",
         .label = "Animation speed",
         .group = "Model",
         .kind = ValueKind::Float,
         .range = {.min = 0.0F, .max = 8.0F, .step = 0.005F, .soft = true},
         .apply = Apply::Rebind,
         .unit = "ticks/frame",
         .help = "Ticks the engine adds to this model's own clip every drawn frame. IIDX RED "
                 "leaves every bound slot at 0.75 and only ever scripts slot 0.",
         .aliases = "clip playback speed"}),
    Row<Scope::Model, &ModelState::scale>({.key = "scale",
                                           .label = "Scale",
                                           .group = "Model",
                                           .kind = ValueKind::Vec3,
                                           .range = {.min = 0.001F, .max = 100.0F, .soft = true},
                                           .apply = Apply::Rebind}),
    Row<Scope::Model, &ModelState::visible>({.key = "visible",
                                             .label = "Visible",
                                             .group = "Model",
                                             .kind = ValueKind::Bool,
                                             .apply = Apply::Rebind,
                                             .help = "Hide a layer without changing the preset.",
                                             .aliases = "show hide"}),
    Row<Scope::Model, &ModelState::position>(
        {.key = "position",
         .label = "Position",
         .group = "Model",
         .kind = ValueKind::Vec3,
         .range = {.min = -1000.0F, .max = 1000.0F, .soft = true},
         .apply = Apply::Live}),
    Row<Scope::Model, &ModelState::rotation>(
        {.key = "rotation",
         .label = "Rotation",
         .group = "Model",
         .kind = ValueKind::Vec3,
         .range = {.min = -50.0F, .max = 50.0F, .step = 0.0005F, .soft = true},
         .apply = Apply::Live,
         .unit = "rad",
         .help = "Base rotation before the per-frame spin accumulates. These are radians, so a "
                 "literal 45 from the game really is 45 radians.",
         .aliases = "orientation angle"}),

    Row<Scope::ModelMotion, &ModelMotion::spin_per_frame>(
        {.key = "motion.spin_per_frame",
         .label = "Spin per frame",
         .group = "Model motion",
         .kind = ValueKind::Vec3,
         .range = {.min = -0.5F, .max = 0.5F, .step = 0.0005F},
         .apply = Apply::Live,
         .unit = "rad/frame",
         .help = "Rotation the screen's per-frame update adds every tick.",
         .aliases = "spin rotate yaw"}),
    Row<Scope::ModelMotion, &ModelMotion::orbit_radius>(
        {.key = "motion.orbit_radius",
         .label = "Orbit radius",
         .group = "Model motion",
         .kind = ValueKind::Float,
         .range = {.min = 0.0F, .max = 100.0F, .step = 0.005F, .soft = true},
         .apply = Apply::Live,
         .help = "Non-zero switches the layer from a fixed position to the orbit path.",
         .aliases = "orbit circle"}),
    Row<Scope::ModelMotion, &ModelMotion::orbit_rate>(
        {.key = "motion.orbit_rate",
         .label = "Orbit rate",
         .group = "Model motion",
         .kind = ValueKind::Float,
         .range = {.min = -1.0F, .max = 1.0F, .step = 0.0005F},
         .apply = Apply::Live,
         .unit = "rad/frame"}),
    Row<Scope::ModelMotion, &ModelMotion::spin_kick>(
        {.key = "motion.spin_kick",
         .label = "Spin kick",
         .group = "Model motion",
         .kind = ValueKind::Float,
         .range = {.min = 0.0F, .max = 64.0F, .step = 0.05F},
         .apply = Apply::Live,
         .help = "Multiplier applied to the spin when a state changes, decaying back to one.",
         .aliases = "kick impulse"}),
    Row<Scope::ModelMotion, &ModelMotion::spin_kick_decay>(
        {.key = "motion.spin_kick_decay",
         .label = "Spin kick decay",
         .group = "Model motion",
         .kind = ValueKind::Float,
         .range = {.min = 0.0F, .max = 8.0F, .step = 0.005F},
         .apply = Apply::Live}),

    Row<Scope::Light, &DirectionalLight::direction>({.key = "direction",
                                                     .label = "Direction",
                                                     .group = "Lighting",
                                                     .kind = ValueKind::Vec3,
                                                     .range = {.min = -1.0F, .max = 1.0F},
                                                     .apply = Apply::Rebind}),
    Row<Scope::Light, &DirectionalLight::diffuse>(
        {.key = "diffuse",
         .label = "Diffuse",
         .group = "Lighting",
         .kind = ValueKind::Color,
         .range = {.min = 0.0F, .max = 4.0F, .soft = true},
         .apply = Apply::Rebind}),
    Row<Scope::Light, &DirectionalLight::specular>(
        {.key = "specular",
         .label = "Specular",
         .group = "Lighting",
         .kind = ValueKind::Color,
         .range = {.min = 0.0F, .max = 4.0F, .soft = true},
         .apply = Apply::Rebind}),

    Row<Scope::Sprite, &SpriteState::visible>({.key = "visible",
                                               .label = "Visible",
                                               .group = "2D layer",
                                               .kind = ValueKind::Bool,
                                               .apply = Apply::Rebind}),
    Row<Scope::Sprite, &SpriteState::x>(
        {.key = "x",
         .label = "X",
         .group = "2D layer",
         .kind = ValueKind::Float,
         .range = {.min = -4096.0F, .max = 4096.0F, .step = 1.0F, .soft = true},
         .apply = Apply::Rebind,
         .unit = "px"}),
    Row<Scope::Sprite, &SpriteState::y>(
        {.key = "y",
         .label = "Y",
         .group = "2D layer",
         .kind = ValueKind::Float,
         .range = {.min = -4096.0F, .max = 4096.0F, .step = 1.0F, .soft = true},
         .apply = Apply::Rebind,
         .unit = "px"}),
    Row<Scope::Sprite, &SpriteState::alpha>({.key = "alpha",
                                             .label = "Alpha",
                                             .group = "2D layer",
                                             .kind = ValueKind::Float,
                                             .range = {.min = 0.0F, .max = 1.0F, .step = 0.005F},
                                             .apply = Apply::Rebind}),
    Row<Scope::Sprite, &SpriteState::blend>({.key = "blend",
                                             .label = "Blend",
                                             .group = "2D layer",
                                             .kind = ValueKind::Enum,
                                             .apply = Apply::Rebind,
                                             .enum_labels = kSpriteBlendLabels}),
    Row<Scope::Sprite, &SpriteState::priority>(
        {.key = "priority",
         .label = "Priority",
         .group = "2D layer",
         .kind = ValueKind::Int,
         .range = {.min = 0.0F, .max = 64.0F, .step = 1.0F},
         .apply = Apply::Rebind,
         .help = "Draw order, and which side of the 3D pass this layer lands on.",
         .aliases = "order depth split"}),
    Row<Scope::Sprite, &SpriteState::scroll_x>(
        {.key = "scroll_x",
         .label = "Scroll rate",
         .group = "2D layer",
         .kind = ValueKind::Float,
         .range = {.min = -64.0F, .max = 64.0F, .step = 0.05F},
         .apply = Apply::Rebind,
         .unit = "px/frame"}),
    Row<Scope::Sprite, &SpriteState::scroll_wrap>(
        {.key = "scroll_wrap",
         .label = "Scroll wrap",
         .group = "2D layer",
         .kind = ValueKind::Float,
         .range = {.min = 0.0F, .max = 8192.0F, .step = 1.0F, .soft = true},
         .apply = Apply::Rebind,
         .unit = "px"}),
    Row<Scope::SpriteTiming, &GcAnim::Timing::playback>(
        {.key = "timing.playback",
         .label = "Playback",
         .group = "2D layer",
         .kind = ValueKind::Enum,
         .apply = Apply::Rebind,
         .help = "What the layer does past the end of its timeline.",
         .aliases = "loop hold end",
         .enum_labels = kPlaybackLabels}),

    Row<Scope::Countdown, &Countdown::start_frames>(
        {.key = "countdown.start_frames",
         .label = "Timer",
         .group = "Timing",
         .kind = ValueKind::Int,
         .range = {.min = 0.0F, .max = 36000.0F, .step = 1.0F},
         .apply = Apply::Live,
         .unit = "frames"}),
    Row<Scope::Countdown, &Countdown::ramp_below>(
        {.key = "countdown.ramp_below",
         .label = "Ramp below",
         .group = "Timing",
         .kind = ValueKind::Int,
         .range = {.min = 0.0F, .max = 36000.0F, .step = 1.0F},
         .apply = Apply::Live,
         .unit = "frames"}),
    Row<Scope::Countdown, &Countdown::speed_per_frame>(
        {.key = "countdown.speed_per_frame",
         .label = "Ramp slope",
         .group = "Timing",
         .kind = ValueKind::Float,
         .range = {.min = -0.5F, .max = 0.5F, .step = 0.0001F},
         .apply = Apply::Live}),
    Row<Scope::Countdown, &Countdown::fade_from>(
        {.key = "countdown.fade_from",
         .label = "Fade from",
         .group = "Timing",
         .kind = ValueKind::Float,
         .range = {.min = 0.0F, .max = 1.0F, .step = 0.005F},
         .apply = Apply::Live}),
    Row<Scope::Countdown, &Countdown::fade_per_frame>(
        {.key = "countdown.fade_per_frame",
         .label = "Fade slope",
         .group = "Timing",
         .kind = ValueKind::Float,
         .range = {.min = 0.0F, .max = 0.05F, .step = 0.00001F},
         .apply = Apply::Live}),
    Row<Scope::Countdown, &Countdown::ramp_blend_mode>({.key = "countdown.ramp_blend_mode",
                                                        .label = "Ramp draw mode",
                                                        .group = "Timing",
                                                        .kind = ValueKind::Enum,
                                                        .apply = Apply::Live,
                                                        .enum_labels = kBlendLabels}),
    Row<Scope::Intro, &Intro::frames>({.key = "intro.frames",
                                       .label = "Intro length",
                                       .group = "Timing",
                                       .kind = ValueKind::Int,
                                       .range = {.min = 0.0F, .max = 600.0F, .step = 1.0F},
                                       .apply = Apply::Live,
                                       .unit = "frames"}),
    Row<Scope::Intro, &Intro::speed_from>({.key = "intro.speed_from",
                                           .label = "Intro speed from",
                                           .group = "Timing",
                                           .kind = ValueKind::Float,
                                           .range = {.min = -16.0F, .max = 16.0F, .step = 0.005F},
                                           .apply = Apply::Live}),
    Row<Scope::Intro, &Intro::speed_to>({.key = "intro.speed_to",
                                         .label = "Intro speed to",
                                         .group = "Timing",
                                         .kind = ValueKind::Float,
                                         .range = {.min = -16.0F, .max = 16.0F, .step = 0.005F},
                                         .apply = Apply::Live}),

    Row<Scope::Option, &OptionState::transition_frames>(
        {.key = "transition_frames",
         .label = "Transition length",
         .group = "States",
         .kind = ValueKind::Int,
         .range = {.min = 0.0F, .max = 600.0F, .step = 1.0F},
         .apply = Apply::Live,
         .unit = "frames"}),
    Row<Scope::Option, &OptionState::transition_step>(
        {.key = "transition_step",
         .label = "Transition step",
         .group = "States",
         .kind = ValueKind::Int,
         .range = {.min = 1.0F, .max = 64.0F, .step = 1.0F},
         .apply = Apply::Live,
         .help = "Frames the transition counter drops each tick. The game's own value is 4, so "
                 "the transition length is really quarter frames.",
         .aliases = "transition rate"}),
    Row<Scope::Option, &OptionState::spin_kick>(
        {.key = "spin_kick",
         .label = "State change kick",
         .group = "States",
         .kind = ValueKind::Float,
         .range = {.min = 0.0F, .max = 64.0F, .step = 0.05F},
         .apply = Apply::Live}),
    Row<Scope::OptionChoice, &ChoiceState::position>(
        {.key = "position",
         .label = "Position",
         .group = "States",
         .kind = ValueKind::Vec3,
         .range = {.min = -1000.0F, .max = 1000.0F, .soft = true},
         .apply = Apply::Live}),
    Row<Scope::OptionChoice, &ChoiceState::camera_eye>(
        {.key = "camera_eye",
         .label = "Camera eye",
         .group = "States",
         .kind = ValueKind::Vec3,
         .range = {.min = -1000.0F, .max = 1000.0F, .soft = true},
         .apply = Apply::Live,
         .help = "Where this state puts the camera, for the screens whose states move the view "
                 "rather than the model.",
         .aliases = "camera view state"}),
});

}

std::span<const ParamDesc> Schema() {
    return kRows;
}

}
