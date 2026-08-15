#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace Preset::Doc {

enum class TrackKind : uint8_t {
    Sprite,
    Model,
    Camera,
    Light,
    Fx,
    Scene,
};

enum class CommandType : uint8_t {
    SpriteDraw,
    SpriteAnimate,
    SpriteScroll,
    Emitter,
    ModelDraw,
    ModelTween,
    ModelMotion,
    CameraSet,
    CameraTween,
    LightSet,
    ParamOverride,
    RenderSettings,
    RngSeed,
    RhythmBeat,
    RhythmJitter,
    OptionSelect,
};

enum class Ease : uint8_t {
    Hold,
    Linear,
    SineDeg,
    EaseIn,
    EaseOut,
    EaseInOut,
    Bezier,
};

enum class AssetKind : uint8_t {
    Scene3d,
    Package2d,
};

enum class Shading : uint8_t {
    TextureOnly,
    LitMaterial,
};

enum class SpriteBlend : uint8_t {
    Normal,
    Additive,
    Subtract,
    Replace,
};

enum class ModelBlend : uint8_t {
    Opaque,
    Opaque1,
    Alpha,
    Additive,
    Subtract,
};

enum class Spawn : uint8_t {
    ClipStart,
    EveryFrame,
    Beat,
};

enum class Grid : uint8_t {
    A,
    B,
};

enum class JitterMode : uint8_t {
    Set,
    Add,
};

enum class ClipClock : uint8_t {
    Continue,
    Restart,
};

enum class GateKind : uint8_t {
    Choice,
    Choices,
    Not,
};

inline constexpr std::array<std::string_view, 6> kTrackKindNames = {"sprite", "model", "camera",
                                                                    "light",  "fx",    "scene"};

inline constexpr std::array<std::string_view, 16> kCommandTypeNames = {
    "sprite.draw",  "sprite.animate", "sprite.scroll",  "emitter",
    "model.draw",   "model.tween",    "model.motion",   "camera.set",
    "camera.tween", "light.set",      "param.override", "render.settings",
    "rng.seed",     "rhythm.beat",    "rhythm.jitter",  "option.select"};

inline constexpr std::array<std::string_view, 7> kEaseNames = {
    "hold", "linear", "sine_deg", "ease_in", "ease_out", "ease_in_out", "bezier"};

inline constexpr std::array<std::string_view, 2> kAssetKindNames = {"scene3d", "package2d"};

inline constexpr std::array<std::string_view, 2> kShadingNames = {"texture_only", "lit_material"};

inline constexpr std::array<std::string_view, 4> kSpriteBlendNames = {"normal", "additive",
                                                                      "subtract", "replace"};

inline constexpr std::array<std::string_view, 5> kModelBlendNames = {"opaque", "opaque_1", "alpha",
                                                                     "additive", "subtract"};

inline constexpr std::array<std::string_view, 3> kPlaybackNames = {"loop", "hold_last",
                                                                   "hide_after_end"};

inline constexpr std::array<std::string_view, 3> kSpawnNames = {"clip_start", "every_frame",
                                                                "beat"};

inline constexpr std::array<std::string_view, 2> kGridNames = {"a", "b"};

inline constexpr std::array<std::string_view, 2> kJitterModeNames = {"set", "add"};

inline constexpr std::array<std::string_view, 2> kClipClockNames = {"continue", "restart"};

bool IndexForName(std::span<const std::string_view> names, std::string_view name, int& index);

std::string_view NameForIndex(std::span<const std::string_view> names, int index);

}
