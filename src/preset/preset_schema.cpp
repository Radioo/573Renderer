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
template <> struct ScopeOwner<Scope::Beat> {
    using type = Beat;
};
template <> struct ScopeOwner<Scope::Pulse> {
    using type = Pulse;
};
template <> struct ScopeOwner<Scope::Jitter> {
    using type = Jitter;
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
constexpr std::array<std::string_view, 2> kGridLabels = {"grid A", "grid B"};

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
    Row<Scope::Sprite, &SpriteState::scale>(
        {.key = "scale",
         .label = "Scale",
         .group = "2D layer",
         .kind = ValueKind::Float,
         .range = {.min = 0.05F, .max = 16.0F, .step = 0.005F, .soft = true},
         .apply = Apply::Rebind,
         .help = "Uniform scale about the 640x480 centre, the point the game blits these "
                 "layers from. RED's ending zooms its backdrop this way over the whole "
                 "timeline.",
         .aliases = "zoom size scale"}),
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
    Row<Scope::Intro, &Intro::fov_from>(
        {.key = "intro.fov_from",
         .label = "Intro fov from",
         .group = "Timing",
         .kind = ValueKind::Float,
         .range = {.min = 0.0F, .max = 64.0F, .soft = true},
         .apply = Apply::Live,
         .unit = "rad",
         .help = "Projection field of view at the start of the intro ramp. Zero means the "
                 "screen does not ramp the projection at all.",
         .aliases = "fov zoom intro"}),
    Row<Scope::Intro, &Intro::fov_to>({.key = "intro.fov_to",
                                       .label = "Intro fov to",
                                       .group = "Timing",
                                       .kind = ValueKind::Float,
                                       .range = {.min = 0.0F, .max = 64.0F, .soft = true},
                                       .apply = Apply::Live,
                                       .unit = "rad",
                                       .aliases = "fov zoom intro"}),
    Row<Scope::Intro, &Intro::speed_to>({.key = "intro.speed_to",
                                         .label = "Intro speed to",
                                         .group = "Timing",
                                         .kind = ValueKind::Float,
                                         .range = {.min = -16.0F, .max = 16.0F, .step = 0.005F},
                                         .apply = Apply::Live}),

    Row<Scope::Scene, &Effective::rng_seed>(
        {.key = "rng.seed",
         .label = "Random seed",
         .group = "Beat and noise",
         .kind = ValueKind::Int,
         .range = {.min = 0.0F, .max = 2000000000.0F, .step = 1.0F, .soft = true},
         .apply = Apply::Live,
         .help = "Seed for the game's own subtractive generator. IIDX RED reseeds it from the "
                 "wall clock when a stage starts, so its ending draws a different scatter every "
                 "run. Pick a seed here and the whole timeline replays identically.",
         .aliases = "random noise particles seed"}),

    Row<Scope::Beat, &Beat::rate>(
        {.key = "beat.rate",
         .label = "Beat rate",
         .group = "Beat and noise",
         .kind = ValueKind::Int,
         .range = {.min = 0.0F, .max = 1000.0F, .step = 1.0F},
         .apply = Apply::Live,
         .help = "Beats counted per beat span. RED's ending uses 155 over 3600 frames, which is "
                 "155 BPM at 60 frames a second. Zero disables the beat entirely.",
         .aliases = "bpm tempo beat"}),
    Row<Scope::Beat, &Beat::span>({.key = "beat.span",
                                   .label = "Beat span",
                                   .group = "Beat and noise",
                                   .kind = ValueKind::Int,
                                   .range = {.min = 1.0F, .max = 36000.0F, .step = 1.0F},
                                   .apply = Apply::Live,
                                   .unit = "frames",
                                   .aliases = "bpm tempo beat"}),
    Row<Scope::Beat, &Beat::offset_a>({.key = "beat.offset_a",
                                       .label = "Beat A offset",
                                       .group = "Beat and noise",
                                       .kind = ValueKind::Int,
                                       .range = {.min = -3600.0F, .max = 3600.0F, .step = 1.0F},
                                       .apply = Apply::Live,
                                       .unit = "frames",
                                       .help = "Frame the first beat grid starts counting from.",
                                       .aliases = "beat phase offset"}),
    Row<Scope::Beat, &Beat::offset_b>(
        {.key = "beat.offset_b",
         .label = "Beat B offset",
         .group = "Beat and noise",
         .kind = ValueKind::Int,
         .range = {.min = -3600.0F, .max = 3600.0F, .step = 1.0F},
         .apply = Apply::Live,
         .unit = "frames",
         .help = "Frame the second beat grid starts counting from. RED runs it eleven frames "
                 "ahead of the first so one section can pulse off the other grid.",
         .aliases = "beat phase offset"}),

    Row<Scope::Pulse, &Pulse::grid>({.key = "pulse.grid",
                                     .label = "Pulse grid",
                                     .group = "Beat and noise",
                                     .kind = ValueKind::Enum,
                                     .apply = Apply::Live,
                                     .help = "Which beat grid the scale pulse fires on.",
                                     .aliases = "beat pulse grid",
                                     .enum_labels = kGridLabels}),
    Row<Scope::Pulse, &Pulse::scale_odd>(
        {.key = "pulse.scale_odd",
         .label = "Pulse on odd beats",
         .group = "Beat and noise",
         .kind = ValueKind::Float,
         .range = {.min = 0.25F, .max = 4.0F, .step = 0.005F},
         .apply = Apply::Live,
         .help = "Model scale on the frame an odd beat lands, decaying back to one over the "
                 "pulse length. One means no pulse.",
         .aliases = "beat pulse scale punch"}),
    Row<Scope::Pulse, &Pulse::scale_even>(
        {.key = "pulse.scale_even",
         .label = "Pulse on even beats",
         .group = "Beat and noise",
         .kind = ValueKind::Float,
         .range = {.min = 0.25F, .max = 4.0F, .step = 0.005F},
         .apply = Apply::Live,
         .help = "Scale on even beats. RED's logo section punches to 1.5 on odd beats and only "
                 "1.2 on even ones, which is what gives it the limp.",
         .aliases = "beat pulse scale punch"}),
    Row<Scope::Pulse, &Pulse::frames>({.key = "pulse.frames",
                                       .label = "Pulse length",
                                       .group = "Beat and noise",
                                       .kind = ValueKind::Int,
                                       .range = {.min = 1.0F, .max = 120.0F, .step = 1.0F},
                                       .apply = Apply::Live,
                                       .unit = "frames",
                                       .aliases = "beat pulse decay"}),

    Row<Scope::Jitter, &Jitter::from_frame>(
        {.key = "jitter.from_frame",
         .label = "Jitter from",
         .group = "Beat and noise",
         .kind = ValueKind::Int,
         .range = {.min = 0.0F, .max = 36000.0F, .step = 1.0F},
         .apply = Apply::Live,
         .unit = "frames",
         .help = "Scene frame the per-frame position jitter starts on.",
         .aliases = "shake noise jitter"}),
    Row<Scope::Jitter, &Jitter::span>(
        {.key = "jitter.span",
         .label = "Jitter span",
         .group = "Beat and noise",
         .kind = ValueKind::Int,
         .range = {.min = 0.0F, .max = 4096.0F, .step = 1.0F},
         .apply = Apply::Live,
         .help = "Width of the random draw. The offset is the draw minus half the span, so the "
                 "shake is centred. Zero switches the jitter off.",
         .aliases = "shake noise jitter"}),
    Row<Scope::Jitter, &Jitter::scale>({.key = "jitter.scale",
                                        .label = "Jitter scale",
                                        .group = "Beat and noise",
                                        .kind = ValueKind::Float,
                                        .range = {.min = 0.0F, .max = 0.01F, .step = 0.000001F},
                                        .apply = Apply::Live,
                                        .help = "World units each step of the draw is worth.",
                                        .aliases = "shake noise jitter"}),

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
