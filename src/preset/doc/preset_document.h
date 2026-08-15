#pragma once

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_enum_names.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Preset::Doc {

inline constexpr int kSchemaVersion = 1;
inline constexpr std::string_view kSchemaId = "573renderer/scene-preset";

struct ExtraKey {
    std::string key = {};
    std::string json = {};
    bool operator==(const ExtraKey&) const = default;
};

struct RenderSpec {
    int width = 640;
    int height = 480;
    bool opaque = true;
    Shading shading = Shading::LitMaterial;
    int sprite_split_priority = 30;
    bool operator==(const RenderSpec&) const = default;
};

struct CameraSpec {
    Vec3 eye = {0.0, 0.0, -1.0};
    Vec3 at = {0.0, 0.0, 0.0};
    Vec3 up = {0.0, 1.0, 0.0};
    double fov_y = 1.0471976;
    double near_z = 0.1;
    double far_z = 500.0;
    AspectSpec aspect = {};
    bool operator==(const CameraSpec&) const = default;
};

struct LightSpec {
    Vec3 direction = {0.0, 0.0, -1.0};
    Vec3 diffuse = {1.0, 1.0, 1.0};
    Vec3 specular = {1.0, 1.0, 1.0};
    bool operator==(const LightSpec&) const = default;
};

struct Asset {
    std::string id = {};
    AssetKind kind = AssetKind::Package2d;
    std::string dir = {};
    bool operator==(const Asset&) const = default;
};

struct Marker {
    int frame = 0;
    std::string label = {};
    bool operator==(const Marker&) const = default;
};

struct Gate {
    std::string option = {};
    GateKind kind = GateKind::Choice;
    std::vector<std::string> choices = {};
    bool operator==(const Gate&) const = default;
};

struct KeyValue {
    std::string id = {};
    ParamValue value = {};
    bool operator==(const KeyValue&) const = default;
};

struct Key {
    int at = 0;
    Ease ease = Ease::Linear;
    std::optional<double> rate_deg = {};
    std::optional<std::array<double, 4>> cp = {};
    std::vector<KeyValue> values = {};
    bool operator==(const Key&) const = default;
};

struct Clip {
    std::string id = {};
    int start = 0;
    std::optional<int> end = {};
    std::optional<Gate> when = {};
    std::string label = {};
    bool muted = false;
    Command command = {};
    std::vector<Key> keys = {};
    std::vector<ExtraKey> params_extra = {};
    std::vector<ExtraKey> extra = {};
    bool operator==(const Clip&) const = default;
};

struct Track {
    std::string id = {};
    std::string name = {};
    TrackKind kind = TrackKind::Scene;
    std::string target = {};
    bool muted = false;
    bool solo = false;
    bool locked = false;
    std::string color = {};
    std::vector<Clip> clips = {};
    std::vector<ExtraKey> extra = {};
    bool operator==(const Track&) const = default;
};

struct Transition {
    int frames = 0;
    int step = 4;
    Ease ease = Ease::Linear;
    double spin_kick = 0.0;
    bool operator==(const Transition&) const = default;
};

struct ChoiceValue {
    std::string id = {};
    OverrideValue value = {};
    bool operator==(const ChoiceValue&) const = default;
};

struct ChoiceSpec {
    std::string label = {};
    std::vector<ChoiceValue> values = {};
    bool operator==(const ChoiceSpec&) const = default;
};

struct OptionSpec {
    std::string id = {};
    std::string label = {};
    int default_choice = 0;
    Transition transition = {};
    std::vector<ChoiceSpec> choices = {};
    bool operator==(const OptionSpec&) const = default;
};

struct Document {
    std::string schema = std::string(kSchemaId);
    int version = kSchemaVersion;
    std::string id = {};
    std::string name = {};
    std::string build = {};
    int fps = 60;
    std::optional<int> length = {};
    RenderSpec render = {};
    CameraSpec camera = {};
    std::vector<LightSpec> lights = {};
    std::vector<Asset> assets = {};
    std::vector<OptionSpec> options = {};
    int rng_seed = 1;
    std::vector<Marker> markers = {};
    std::vector<Track> tracks = {};
    std::string notes = {};
    std::vector<ExtraKey> extra = {};
    bool operator==(const Document&) const = default;
};

inline bool HasTarget(TrackKind kind) {
    return kind == TrackKind::Sprite || kind == TrackKind::Model;
}

}
