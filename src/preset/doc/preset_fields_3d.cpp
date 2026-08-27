#include "preset/doc/preset_fields_table.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_fields.h"

#include <array>
#include <span>

namespace Preset::Doc {

namespace {

constexpr Range kWorld = {.min = -1000.0F, .max = 1000.0F, .soft = true};
constexpr Range kSpin = {.min = -0.5F, .max = 0.5F, .step = 0.0005F};
constexpr Range kUnitVector = {.min = -1.0F, .max = 1.0F};
constexpr Range kColor = {.min = 0.0F, .max = 4.0F, .soft = true};
constexpr Range kEaseRate = {.min = 0.0F, .max = 1.0F, .step = 0.001F};
constexpr Range kTileCount = {.min = 1.0F, .max = 16.0F, .step = 1.0F};
constexpr Range kLatticeSeed = {.min = 0.0F, .max = 2000000000.0F, .step = 1.0F, .soft = true};
constexpr Range kTexels = {.min = 1.0F, .max = 4096.0F, .step = 1.0F};

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
         .range = {.min = 0.0F, .max = 7.0F, .step = 1.0F},
         .help = "Light slot the clip drives, one of the eight fixed-function slots."}),
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
    Field<LightSet, &LightSet::specular>(
        {.id = "specular",
         .kind = FieldKind::Vec3,
         .range = kColor,
         .unit = "rgb",
         .help = "Specular colour. Inert on models: the renderer leaves SPECULARENABLE off, as "
                 "the game does.",
         .tweenable = true}),
    Field<LightSet, &LightSet::ambient>({.id = "ambient",
                                         .kind = FieldKind::Vec3,
                                         .range = kColor,
                                         .unit = "rgb",
                                         .help = "Ambient colour the slot adds everywhere.",
                                         .tweenable = true}),
    Field<LightSet, &LightSet::enabled>(
        {.id = "enabled", .kind = FieldKind::Bool, .help = "Whether the slot is lit."}),
});

constexpr auto kCameraEaseFields = std::to_array<FieldDesc>({
    Field<CameraEaseCmd, &CameraEaseCmd::eye_target>({.id = "eye_target",
                                                      .kind = FieldKind::Vec3,
                                                      .range = kWorld,
                                                      .unit = "world",
                                                      .help = "Point the eye eases toward."}),
    Field<CameraEaseCmd, &CameraEaseCmd::at_target>({.id = "at_target",
                                                     .kind = FieldKind::Vec3,
                                                     .range = kWorld,
                                                     .unit = "world",
                                                     .help = "Point the look-at eases toward."}),
    Field<CameraEaseCmd, &CameraEaseCmd::rate>(
        {.id = "rate",
         .kind = FieldKind::Float,
         .range = kEaseRate,
         .help = "Fraction of the remaining distance closed each frame."}),
    Field<CameraEaseCmd, &CameraEaseCmd::eye_x>(
        {.id = "eye_x", .kind = FieldKind::Bool, .help = "Whether the eye X follows its target."}),
    Field<CameraEaseCmd, &CameraEaseCmd::eye_y>(
        {.id = "eye_y", .kind = FieldKind::Bool, .help = "Whether the eye Y follows its target."}),
    Field<CameraEaseCmd, &CameraEaseCmd::eye_z>(
        {.id = "eye_z", .kind = FieldKind::Bool, .help = "Whether the eye Z follows its target."}),
    Field<CameraEaseCmd, &CameraEaseCmd::at_x>(
        {.id = "at_x",
         .kind = FieldKind::Bool,
         .help = "Whether the look-at X follows its target."}),
    Field<CameraEaseCmd, &CameraEaseCmd::at_y>(
        {.id = "at_y",
         .kind = FieldKind::Bool,
         .help = "Whether the look-at Y follows its target."}),
    Field<CameraEaseCmd, &CameraEaseCmd::at_z>(
        {.id = "at_z",
         .kind = FieldKind::Bool,
         .help = "Whether the look-at Z follows its target."}),
    Field<CameraEaseCmd, &CameraEaseCmd::start_at_target>(
        {.id = "start_at_target",
         .kind = FieldKind::Bool,
         .help = "Seed the ease at its target the first time any camera.ease arms."}),
});

constexpr auto kModelEaseFields = std::to_array<FieldDesc>({
    Field<ModelEaseCmd, &ModelEaseCmd::scale_target>({.id = "scale_target",
                                                      .kind = FieldKind::Vec3,
                                                      .range = kWorld,
                                                      .help = "Scale the model eases toward."}),
    Field<ModelEaseCmd, &ModelEaseCmd::position_target>(
        {.id = "position_target",
         .kind = FieldKind::Vec3,
         .range = kWorld,
         .unit = "world",
         .help = "Position the model eases toward."}),
    Field<ModelEaseCmd, &ModelEaseCmd::alpha_target>({.id = "alpha_target",
                                                      .kind = FieldKind::Float,
                                                      .range = kUnitInterval,
                                                      .help = "Alpha the model eases toward."}),
    Field<ModelEaseCmd, &ModelEaseCmd::rate>(
        {.id = "rate",
         .kind = FieldKind::Float,
         .range = kEaseRate,
         .help = "Geometric fraction closed, or the linear step, each frame."}),
    Field<ModelEaseCmd, &ModelEaseCmd::mode>(
        {.id = "mode",
         .kind = FieldKind::Enum,
         .help = "Geometric closes a fraction of the gap, linear steps by rate and stops.",
         .enum_names = kEaseModeNames}),
    Field<ModelEaseCmd, &ModelEaseCmd::start_at_target>(
        {.id = "start_at_target",
         .kind = FieldKind::Bool,
         .help = "Seed the ease at its target the first time any model.ease arms this model."}),
});

constexpr auto kCameraMotionFields = std::to_array<FieldDesc>({
    Field<CameraMotionCmd, &CameraMotionCmd::up_roll_deg_per_frame>(
        {.id = "up_roll_deg_per_frame",
         .kind = FieldKind::Float,
         .range = kDegrees,
         .unit = "deg/frame",
         .help = "Degrees the up vector turns about the view axis through the eye each frame, "
                 "from +X toward -Y. It integrates and is never reset.",
         .tweenable = true}),
});

constexpr auto kPolyTileGridFields = std::to_array<FieldDesc>({
    Field<PolyTileGrid, &PolyTileGrid::rows>({.id = "rows",
                                              .kind = FieldKind::Int,
                                              .range = kTileCount,
                                              .help = "Tile rows in the grid."}),
    Field<PolyTileGrid, &PolyTileGrid::cols>({.id = "cols",
                                              .kind = FieldKind::Int,
                                              .range = kTileCount,
                                              .help = "Tile columns in the grid."}),
    Field<PolyTileGrid, &PolyTileGrid::lattice_amplitude>(
        {.id = "lattice_amplitude",
         .kind = FieldKind::Float,
         .range = kUnitInterval,
         .help = "Step the one-time lattice jitter draws from. Each drawn point moves by half "
                 "this, either way."}),
    Field<PolyTileGrid, &PolyTileGrid::lattice_seed>(
        {.id = "lattice_seed",
         .kind = FieldKind::Int,
         .range = kLatticeSeed,
         .help = "Seed of the CRT generator the lattice jitter draws from. The game seeds it from "
                 "the clock, so a preset has to name one."}),
    Field<PolyTileGrid, &PolyTileGrid::spacing>(
        {.id = "spacing",
         .kind = FieldKind::Vec2,
         .range = kWorld,
         .unit = "world",
         .help = "Pitch between neighbouring tile centres."}),
    Field<PolyTileGrid, &PolyTileGrid::depth>({.id = "depth",
                                               .kind = FieldKind::Float,
                                               .range = kWorld,
                                               .unit = "world",
                                               .help = "Z the grid is built at and orbits about."}),
    Field<PolyTileGrid, &PolyTileGrid::quad_scale>(
        {.id = "quad_scale",
         .kind = FieldKind::Vec2,
         .range = kWorld,
         .unit = "world",
         .help = "World size one unit of lattice space is worth."}),
    Field<PolyTileGrid, &PolyTileGrid::spin_rates>(
        {.id = "spin_rates",
         .kind = FieldKind::Vec3,
         .range = kDegrees,
         .unit = "deg/frame",
         .help = "Multipliers of the fixed spin parity patterns, about each tile's own centre."}),
    Field<PolyTileGrid, &PolyTileGrid::orbit_rates>(
        {.id = "orbit_rates",
         .kind = FieldKind::Vec3,
         .range = kDegrees,
         .unit = "deg/frame",
         .help = "Multipliers of the fixed orbit parity patterns, about the grid centre."}),
    Field<PolyTileGrid, &PolyTileGrid::burst_from>(
        {.id = "burst_from",
         .kind = FieldKind::Int,
         .range = kFrameCount,
         .unit = "frames",
         .help = "Document frame the burst counter starts climbing at."}),
    Field<PolyTileGrid, &PolyTileGrid::burst_step>(
        {.id = "burst_step",
         .kind = FieldKind::Float,
         .range = kWorld,
         .unit = "world/frame",
         .help = "World units the burst counter climbs each frame past burst_from."}),
    Field<PolyTileGrid, &PolyTileGrid::burst_delay_per_tile>(
        {.id = "burst_delay_per_tile",
         .kind = FieldKind::Float,
         .range = kWorld,
         .help = "Counter each further tile waits for before it starts moving in +Z."}),
    Field<PolyTileGrid, &PolyTileGrid::alpha>({.id = "alpha",
                                               .kind = FieldKind::Float,
                                               .range = kUnitInterval,
                                               .help = "Vertex diffuse alpha of every tile.",
                                               .tweenable = true}),
    Field<PolyTileGrid, &PolyTileGrid::texture>(
        {.id = "texture",
         .kind = FieldKind::MovieField,
         .help = "Movie file the tiles are textured with, null for untextured tiles."}),
    Field<PolyTileGrid, &PolyTileGrid::movie_size>(
        {.id = "movie_size",
         .kind = FieldKind::Vec2,
         .range = kTexels,
         .unit = "px",
         .help = "Size of the decoded movie image inside the texture."}),
    Field<PolyTileGrid, &PolyTileGrid::texture_size>(
        {.id = "texture_size",
         .kind = FieldKind::Float,
         .range = kTexels,
         .unit = "px",
         .help = "Side of the square texture the movie image is uploaded into."}),
});

}

std::span<const FieldDesc> ModelDrawFields() {
    return kModelDrawFields;
}

std::span<const FieldDesc> ModelMotionFields() {
    return kModelMotionFields;
}

std::span<const FieldDesc> CameraSetFields() {
    return kCameraSetFields;
}

std::span<const FieldDesc> LightSetFields() {
    return kLightSetFields;
}

std::span<const FieldDesc> CameraEaseFields() {
    return kCameraEaseFields;
}

std::span<const FieldDesc> ModelEaseFields() {
    return kModelEaseFields;
}

std::span<const FieldDesc> CameraMotionFields() {
    return kCameraMotionFields;
}

std::span<const FieldDesc> PolyTileGridFields() {
    return kPolyTileGridFields;
}

}
