#include "preset/doc/preset_fields.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_fields_table.h"

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <tuple>
#include <variant>

namespace Preset::Doc {

namespace {

constexpr Range kFogPlane = {.min = 0.0F, .max = 100000.0F, .step = 0.1F, .soft = true};
constexpr Range kCycleFrames = {.min = 0.0F, .max = 36000.0F, .step = 1.0F};
constexpr Range kLevel = {.min = 0.0F, .max = 255.0F, .step = 1.0F};

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
    Field<RenderSettingsCmd, &RenderSettingsCmd::clear_color>(
        {.id = "clear_color",
         .kind = FieldKind::Vec3,
         .range = kUnitInterval,
         .unit = "rgb",
         .help = "Colour the frame is cleared with.",
         .tweenable = true}),
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

constexpr auto kFogFields = std::to_array<FieldDesc>({
    Field<FogCmd, &FogCmd::enabled>({.id = "enabled",
                                     .kind = FieldKind::Bool,
                                     .help = "Whether the model passes are drawn with fog on."}),
    Field<FogCmd, &FogCmd::color>({.id = "color",
                                   .kind = FieldKind::Vec3,
                                   .range = kUnitInterval,
                                   .unit = "rgb",
                                   .help = "Colour distance fades the models into.",
                                   .tweenable = true}),
    Field<FogCmd, &FogCmd::start>({.id = "start",
                                   .kind = FieldKind::Float,
                                   .range = kFogPlane,
                                   .unit = "world",
                                   .help = "Distance the linear fade begins at.",
                                   .tweenable = true}),
    Field<FogCmd, &FogCmd::end>({.id = "end",
                                 .kind = FieldKind::Float,
                                 .range = kFogPlane,
                                 .unit = "world",
                                 .help = "Distance the fade reaches the fog colour at.",
                                 .tweenable = true}),
    Field<FogCmd, &FogCmd::density>(
        {.id = "density",
         .kind = FieldKind::Float,
         .range = kUnitInterval,
         .help = "Carried to the device but inert: the fade is linear, not exponential.",
         .tweenable = true}),
});

constexpr auto kClearCycleFields = std::to_array<FieldDesc>({
    Field<ClearCycleCmd, &ClearCycleCmd::base>({.id = "base",
                                                .kind = FieldKind::Vec3,
                                                .range = kLevel,
                                                .unit = "levels",
                                                .help = "Colour outside every window, 0 to 255."}),
    Field<ClearCycleCmd, &ClearCycleCmd::strobe_color>(
        {.id = "strobe_color",
         .kind = FieldKind::Vec3,
         .range = kLevel,
         .unit = "levels",
         .help = "Colour a strobe window flashes, 0 to 255."}),
    Field<ClearCycleCmd, &ClearCycleCmd::strobe_period>(
        {.id = "strobe_period",
         .kind = FieldKind::Int,
         .range = kCycleFrames,
         .unit = "frames",
         .help = "Frames one strobe cycle covers, 0 for no strobe."}),
    Field<ClearCycleCmd, &ClearCycleCmd::strobe_window_a>(
        {.id = "strobe_window_a",
         .kind = FieldKind::Int,
         .range = kCycleFrames,
         .unit = "frames",
         .help = "Length of the window that opens the strobe cycle."}),
    Field<ClearCycleCmd, &ClearCycleCmd::strobe_window_b_offset>(
        {.id = "strobe_window_b_offset",
         .kind = FieldKind::Int,
         .range = kCycleFrames,
         .unit = "frames",
         .help = "Frames added before the second window's modulo."}),
    Field<ClearCycleCmd, &ClearCycleCmd::strobe_window_b>({.id = "strobe_window_b",
                                                           .kind = FieldKind::Int,
                                                           .range = kCycleFrames,
                                                           .unit = "frames",
                                                           .help = "Length of the second window."}),
    Field<ClearCycleCmd, &ClearCycleCmd::strobe_skip_every>(
        {.id = "strobe_skip_every",
         .kind = FieldKind::Int,
         .range = {.min = 0.0F, .max = 64.0F, .step = 1.0F},
         .unit = "frames",
         .help = "Frames whose number divides by this stay unlit, 0 to flash every frame."}),
    Field<ClearCycleCmd, &ClearCycleCmd::ramp_period>(
        {.id = "ramp_period",
         .kind = FieldKind::Int,
         .range = kCycleFrames,
         .unit = "frames",
         .help = "Frames one ramp cycle covers, 0 for no ramp."}),
    Field<ClearCycleCmd, &ClearCycleCmd::ramp_length>(
        {.id = "ramp_length",
         .kind = FieldKind::Int,
         .range = kCycleFrames,
         .unit = "frames",
         .help = "Frames the ramp falls over, from the cycle start."}),
    Field<ClearCycleCmd, &ClearCycleCmd::ramp_peak>(
        {.id = "ramp_peak",
         .kind = FieldKind::Int,
         .range = kLevel,
         .unit = "levels",
         .help = "Grey level the ramp starts at, 0 to 255."}),
});

constexpr std::array<FieldDesc, 0> kNoFields = {};

std::array<std::span<const FieldDesc>, 22> BuildTables() {
    return {
        SpriteDrawFields(),
        SpriteAnimateFields(),
        SpriteScrollFields(),
        EmitterFields(),
        ModelDrawFields(),
        kNoFields,
        ModelMotionFields(),
        CameraSetFields(),
        kNoFields,
        LightSetFields(),
        kParamOverrideFields,
        kRenderSettingsFields,
        kRngSeedFields,
        kRhythmBeatFields,
        kRhythmJitterFields,
        kOptionSelectFields,
        kFogFields,
        kClearCycleFields,
        CameraEaseFields(),
        ModelEaseFields(),
        CameraMotionFields(),
        PolyTileGridFields(),
    };
}

static_assert(std::tuple_size_v<decltype(BuildTables())> == std::variant_size_v<Command>);
static_assert(kCommandTypeNames.size() == std::variant_size_v<Command>);
static_assert(kCommandTraits.size() == std::variant_size_v<Command>);

}

std::span<const FieldDesc> FieldsFor(CommandType type) {
    static const std::array<std::span<const FieldDesc>, 22> tables = BuildTables();
    return tables[(std::size_t)type];
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
    static const std::array<Command, 22> defaults = {
        Command{SpriteDraw{}},     Command{SpriteAnimate{}},    Command{SpriteScroll{}},
        Command{EmitterCmd{}},     Command{ModelDraw{}},        Command{ModelTween{}},
        Command{ModelMotionCmd{}}, Command{CameraSet{}},        Command{CameraTween{}},
        Command{LightSet{}},       Command{ParamOverrideCmd{}}, Command{RenderSettingsCmd{}},
        Command{RngSeed{}},        Command{RhythmBeat{}},       Command{RhythmJitter{}},
        Command{OptionSelect{}},   Command{FogCmd{}},           Command{ClearCycleCmd{}},
        Command{CameraEaseCmd{}},  Command{ModelEaseCmd{}},     Command{CameraMotionCmd{}},
        Command{PolyTileGrid{}},
    };
    static_assert(defaults.size() == std::variant_size_v<Command>);
    return defaults[(std::size_t)type];
}

}
