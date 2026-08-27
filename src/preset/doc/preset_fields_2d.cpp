#include "preset/doc/preset_fields_table.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_fields.h"

#include <array>
#include <span>

namespace Preset::Doc {

namespace {

constexpr Range kPixels = {.min = -4096.0F, .max = 4096.0F, .step = 1.0F, .soft = true};
constexpr Range kSpriteScale = {.min = 0.05F, .max = 16.0F, .step = 0.005F, .soft = true};
constexpr Range kRadius = {.min = 0.0F, .max = 8192.0F, .step = 1.0F, .soft = true};

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
    Field<EmitterCmd, &EmitterCmd::burst>(
        {.id = "burst",
         .kind = FieldKind::BurstField,
         .help = "Rising burst rule the burst spawn reads, null for the other spawn rules."}),
});

}

std::span<const FieldDesc> SpriteDrawFields() {
    return kSpriteDrawFields;
}

std::span<const FieldDesc> SpriteAnimateFields() {
    return kSpriteAnimateFields;
}

std::span<const FieldDesc> SpriteScrollFields() {
    return kSpriteScrollFields;
}

std::span<const FieldDesc> EmitterFields() {
    return kEmitterFields;
}

}
