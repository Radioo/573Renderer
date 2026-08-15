#include "preset/doc/preset_json.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_fields.h"
#include "support/expected.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Preset::Doc {

namespace {

using Json = nlohmann::ordered_json;

struct ParseFail {
    ParseError error;
};

[[noreturn]] void Fail(std::string_view path, const std::string& message) {
    throw ParseFail{
        ParseError{.line = 0, .column = 0, .path = std::string(path), .message = message}};
}

const Json* Member(const Json& object, std::string_view key) {
    const auto found = object.find(std::string(key));
    if (found == object.end()) return nullptr;
    return &found.value();
}

const Json& Required(const Json& object, std::string_view key, std::string_view path) {
    const Json* value = Member(object, key);
    if (value == nullptr) Fail(path, "missing key \"" + std::string(key) + "\"");
    return *value;
}

int ReadInt(const Json& value, std::string_view path, std::string_view what) {
    if (!value.is_number_integer()) Fail(path, std::string(what) + " must be a whole number");
    return value.get<int>();
}

double ReadNumber(const Json& value, std::string_view path, std::string_view what) {
    if (!value.is_number()) Fail(path, std::string(what) + " must be a number");
    return value.get<double>();
}

bool ReadBool(const Json& value, std::string_view path, std::string_view what) {
    if (!value.is_boolean()) Fail(path, std::string(what) + " must be true or false");
    return value.get<bool>();
}

std::string ReadString(const Json& value, std::string_view path, std::string_view what) {
    if (!value.is_string()) Fail(path, std::string(what) + " must be a string");
    return value.get<std::string>();
}

int ReadEnum(const Json& value, std::span<const std::string_view> names, std::string_view path,
             std::string_view what) {
    const std::string name = ReadString(value, path, what);
    int index = 0;
    if (!IndexForName(names, name, index)) {
        Fail(path, std::string(what) + " has unknown value \"" + name + "\"");
    }
    return index;
}

template <std::size_t N>
std::array<double, N> ReadArray(const Json& value, std::string_view path, std::string_view what) {
    if (!value.is_array() || value.size() != N) {
        Fail(path, std::string(what) + " must be an array of " + std::to_string(N) + " numbers");
    }
    std::array<double, N> out = {};
    for (std::size_t i = 0; i < N; ++i)
        out[i] = ReadNumber(value[i], path, what);
    return out;
}

std::vector<std::string> ReadStringList(const Json& value, std::string_view path,
                                        std::string_view what) {
    if (!value.is_array()) Fail(path, std::string(what) + " must be an array of strings");
    std::vector<std::string> out;
    out.reserve(value.size());
    for (const Json& item : value)
        out.push_back(ReadString(item, path, what));
    return out;
}

std::vector<ExtraKey> CollectExtra(const Json& object, std::span<const std::string_view> known) {
    std::vector<ExtraKey> extra;
    for (const auto& item : object.items()) {
        bool seen = false;
        for (const std::string_view key : known)
            seen = seen || key == item.key();
        if (!seen) extra.push_back(ExtraKey{.key = item.key(), .json = item.value().dump()});
    }
    return extra;
}

void WriteExtra(Json& object, const std::vector<ExtraKey>& extra) {
    for (const ExtraKey& entry : extra)
        object[entry.key] = Json::parse(entry.json);
}

Json WriteArray(std::span<const double> values) {
    Json out = Json::array();
    for (const double value : values)
        out.push_back(value);
    return out;
}

Orbit ReadOrbit(const Json& value, std::string_view path) {
    Orbit orbit;
    orbit.radius = ReadNumber(Required(value, "radius", path), path, "orbit.radius");
    orbit.rate_rad_per_frame =
        ReadNumber(Required(value, "rate_rad_per_frame", path), path, "orbit.rate_rad_per_frame");
    orbit.center = ReadArray<2>(Required(value, "center", path), path, "orbit.center");
    orbit.z_start = ReadNumber(Required(value, "z_start", path), path, "orbit.z_start");
    orbit.z_per_frame = ReadNumber(Required(value, "z_per_frame", path), path, "orbit.z_per_frame");
    orbit.z_min = ReadNumber(Required(value, "z_min", path), path, "orbit.z_min");
    return orbit;
}

Json WriteOrbit(const Orbit& orbit) {
    Json out = Json::object();
    out["radius"] = orbit.radius;
    out["rate_rad_per_frame"] = orbit.rate_rad_per_frame;
    out["center"] = WriteArray(orbit.center);
    out["z_start"] = orbit.z_start;
    out["z_per_frame"] = orbit.z_per_frame;
    out["z_min"] = orbit.z_min;
    return out;
}

PulseSpec ReadPulse(const Json& value, std::string_view path) {
    PulseSpec pulse;
    pulse.grid = (Grid)ReadEnum(Required(value, "grid", path), kGridNames, path, "pulse.grid");
    pulse.scale_odd = ReadNumber(Required(value, "scale_odd", path), path, "pulse.scale_odd");
    pulse.scale_even = ReadNumber(Required(value, "scale_even", path), path, "pulse.scale_even");
    pulse.frames = ReadInt(Required(value, "frames", path), path, "pulse.frames");
    return pulse;
}

Json WritePulse(const PulseSpec& pulse) {
    Json out = Json::object();
    out["grid"] = std::string(NameForIndex(kGridNames, (int)pulse.grid));
    out["scale_odd"] = pulse.scale_odd;
    out["scale_even"] = pulse.scale_even;
    out["frames"] = pulse.frames;
    return out;
}

Scatter ReadScatter(const Json& value, std::string_view path) {
    Scatter scatter;
    scatter.span = ReadArray<2>(Required(value, "span", path), path, "scatter.span");
    scatter.offset = ReadArray<2>(Required(value, "offset", path), path, "scatter.offset");
    return scatter;
}

Json WriteScatter(const Scatter& scatter) {
    Json out = Json::object();
    out["span"] = WriteArray(scatter.span);
    out["offset"] = WriteArray(scatter.offset);
    return out;
}

OverrideValue ReadOverride(const Json& value, std::string_view path, std::string_view what) {
    if (value.is_boolean()) return value.get<bool>();
    if (value.is_number()) return value.get<double>();
    if (value.is_string()) return value.get<std::string>();
    if (value.is_array() && value.size() == 3) return ReadArray<3>(value, path, what);
    Fail(path, std::string(what) + " must be a bool, number, name or three numbers");
}

ParamValue AsParam(const OverrideValue& value) {
    return std::visit([](const auto& held) { return ParamValue{held}; }, value);
}

ParamValue ReadLooseValue(const Json& value, std::string_view path, std::string_view what) {
    if (value.is_boolean()) return value.get<bool>();
    if (value.is_number_integer()) return value.get<int>();
    if (value.is_number()) return value.get<double>();
    if (value.is_string()) return value.get<std::string>();
    if (value.is_array() && value.size() == 2) return ReadArray<2>(value, path, what);
    if (value.is_array() && value.size() == 3) return ReadArray<3>(value, path, what);
    Fail(path, std::string(what) + " is not a value the document model can hold");
}

ParamValue ReadParam(const FieldDesc& field, const Json& value, std::string_view path) {
    switch (field.kind) {
    case FieldKind::Bool:
        return ReadBool(value, path, field.id);
    case FieldKind::Int:
        return ReadInt(value, path, field.id);
    case FieldKind::Float:
        return ReadNumber(value, path, field.id);
    case FieldKind::Enum:
        return ReadEnum(value, field.enum_names, path, field.id);
    case FieldKind::String:
        return ReadString(value, path, field.id);
    case FieldKind::Vec2:
        return ReadArray<2>(value, path, field.id);
    case FieldKind::Vec3:
        return ReadArray<3>(value, path, field.id);
    case FieldKind::StringList:
        return ReadStringList(value, path, field.id);
    case FieldKind::Aspect:
        if (value.is_string()) {
            if (value.get<std::string>() != "auto") Fail(path, "aspect must be a number or auto");
            return AspectSpec{.automatic = true, .value = 0.0};
        }
        return AspectSpec{.automatic = false, .value = ReadNumber(value, path, field.id)};
    case FieldKind::Clock:
        if (value.is_null()) return std::optional<ClipClock>{};
        return std::optional<ClipClock>{
            (ClipClock)ReadEnum(value, kClipClockNames, path, field.id)};
    case FieldKind::ClipTimeField:
        if (value.is_number_integer()) {
            return ClipTime{.clock = ClipClock::Restart, .ticks = value.get<int>()};
        }
        return ClipTime{.clock = (ClipClock)ReadEnum(value, kClipClockNames, path, field.id),
                        .ticks = std::nullopt};
    case FieldKind::OrbitField:
        if (value.is_null()) return std::optional<Orbit>{};
        return std::optional<Orbit>{ReadOrbit(value, path)};
    case FieldKind::PulseField:
        if (value.is_null()) return std::optional<PulseSpec>{};
        return std::optional<PulseSpec>{ReadPulse(value, path)};
    case FieldKind::ScatterField:
        if (value.is_null()) return std::optional<Scatter>{};
        return std::optional<Scatter>{ReadScatter(value, path)};
    case FieldKind::OverrideField:
        return AsParam(ReadOverride(value, path, field.id));
    }
    Fail(path, "parameter " + std::string(field.id) + " has no reader");
}

Json WriteLooseValue(const ParamValue& value) {
    if (const auto* held = std::get_if<bool>(&value)) return *held;
    if (const auto* held = std::get_if<int>(&value)) return *held;
    if (const auto* held = std::get_if<double>(&value)) return *held;
    if (const auto* held = std::get_if<std::string>(&value)) return *held;
    if (const auto* held = std::get_if<Vec2>(&value)) return WriteArray(*held);
    if (const auto* held = std::get_if<Vec3>(&value)) return WriteArray(*held);
    return {};
}

Json WriteParam(const FieldDesc& field, const ParamValue& value) {
    switch (field.kind) {
    case FieldKind::Enum:
        return std::string(NameForIndex(field.enum_names, std::get<int>(value)));
    case FieldKind::StringList:
        return std::get<std::vector<std::string>>(value);
    case FieldKind::Aspect: {
        const AspectSpec aspect = std::get<AspectSpec>(value);
        if (aspect.automatic) return "auto";
        return aspect.value;
    }
    case FieldKind::Clock: {
        const auto clock = std::get<std::optional<ClipClock>>(value);
        if (!clock.has_value()) return {};
        return std::string(NameForIndex(kClipClockNames, (int)*clock));
    }
    case FieldKind::ClipTimeField: {
        const ClipTime clip_time = std::get<ClipTime>(value);
        if (clip_time.ticks.has_value()) return *clip_time.ticks;
        return std::string(NameForIndex(kClipClockNames, (int)clip_time.clock));
    }
    case FieldKind::OrbitField: {
        const auto orbit = std::get<std::optional<Orbit>>(value);
        if (!orbit.has_value()) return {};
        return WriteOrbit(*orbit);
    }
    case FieldKind::PulseField: {
        const auto pulse = std::get<std::optional<PulseSpec>>(value);
        if (!pulse.has_value()) return {};
        return WritePulse(*pulse);
    }
    case FieldKind::ScatterField: {
        const auto scatter = std::get<std::optional<Scatter>>(value);
        if (!scatter.has_value()) return {};
        return WriteScatter(*scatter);
    }
    default:
        return WriteLooseValue(value);
    }
}

Command ReadCommand(CommandType type, const Json* params, std::string_view path,
                    std::vector<ExtraKey>& extra) {
    Command command = DefaultCommand(type);
    if (params == nullptr) return command;
    if (!params->is_object()) Fail(path, "params must be an object");
    const std::span<const FieldDesc> fields = FieldsFor(type);
    std::vector<std::string_view> known;
    known.reserve(fields.size());
    for (const FieldDesc& field : fields)
        known.push_back(field.id);
    for (const auto& item : params->items()) {
        const FieldDesc* field = FindField(fields, item.key());
        if (field == nullptr) continue;
        field->set(command, ReadParam(*field, item.value(), path));
    }
    extra = CollectExtra(*params, known);
    return command;
}

Json WriteCommand(const Command& command, const std::vector<ExtraKey>& extra) {
    const CommandType type = TypeOf(command);
    const Command& fallback = DefaultCommand(type);
    Json params = Json::object();
    for (const FieldDesc& field : FieldsFor(type)) {
        const ParamValue value = field.get(command);
        if (!field.required && value == field.get(fallback)) continue;
        params[std::string(field.id)] = WriteParam(field, value);
    }
    WriteExtra(params, extra);
    return params;
}

Key ReadKey(const Json& value, CommandType type, std::string_view path) {
    if (!value.is_object()) Fail(path, "every key must be an object");
    Key key;
    key.at = ReadInt(Required(value, "at", path), path, "key at");
    if (const Json* ease = Member(value, "ease")) {
        key.ease = (Ease)ReadEnum(*ease, kEaseNames, path, "key ease");
    }
    if (const Json* rate = Member(value, "rate_deg")) {
        key.rate_deg = ReadNumber(*rate, path, "key rate_deg");
    }
    if (const Json* control = Member(value, "cp")) {
        key.cp = ReadArray<4>(*control, path, "key cp");
    }
    const Json* values = Member(value, "values");
    if (values == nullptr) return key;
    if (!values->is_object()) Fail(path, "key values must be an object");
    const std::span<const FieldDesc> fields = KeyFieldsFor(type);
    for (const auto& item : values->items()) {
        const FieldDesc* field = FindField(fields, item.key());
        const ParamValue parsed = field != nullptr ? ReadParam(*field, item.value(), path)
                                                   : ReadLooseValue(item.value(), path, item.key());
        key.values.push_back(KeyValue{.id = item.key(), .value = parsed});
    }
    return key;
}

Json WriteKey(const Key& key, CommandType type, bool last) {
    Json out = Json::object();
    out["at"] = key.at;
    if (!last || key.ease != Ease::Linear) {
        out["ease"] = std::string(NameForIndex(kEaseNames, (int)key.ease));
    }
    if (key.rate_deg.has_value()) out["rate_deg"] = *key.rate_deg;
    if (key.cp.has_value()) out["cp"] = WriteArray(*key.cp);
    if (key.values.empty()) return out;
    const std::span<const FieldDesc> fields = KeyFieldsFor(type);
    Json values = Json::object();
    for (const KeyValue& value : key.values) {
        const FieldDesc* field = FindField(fields, value.id);
        values[value.id] =
            field != nullptr ? WriteParam(*field, value.value) : WriteLooseValue(value.value);
    }
    out["values"] = values;
    return out;
}

Gate ReadGate(const Json& value, std::string_view path) {
    if (!value.is_object()) Fail(path, "when must be an object");
    Gate gate;
    gate.option = ReadString(Required(value, "option", path), path, "when option");
    if (const Json* choice = Member(value, "choice")) {
        gate.kind = GateKind::Choice;
        gate.choices.push_back(ReadString(*choice, path, "when choice"));
        return gate;
    }
    if (const Json* choices = Member(value, "choices")) {
        gate.kind = GateKind::Choices;
        gate.choices = ReadStringList(*choices, path, "when choices");
        return gate;
    }
    if (const Json* negated = Member(value, "not")) {
        gate.kind = GateKind::Not;
        gate.choices.push_back(ReadString(*negated, path, "when not"));
        return gate;
    }
    Fail(path, "when needs one of choice, choices or not");
}

Json WriteGate(const Gate& gate) {
    Json out = Json::object();
    out["option"] = gate.option;
    const std::string first = gate.choices.empty() ? std::string() : gate.choices.front();
    if (gate.kind == GateKind::Choice) out["choice"] = first;
    if (gate.kind == GateKind::Choices) out["choices"] = gate.choices;
    if (gate.kind == GateKind::Not) out["not"] = first;
    return out;
}

constexpr std::array<std::string_view, 9> kClipKeys = {"id",    "type",  "start",  "end", "when",
                                                       "label", "muted", "params", "keys"};

Clip ReadClip(const Json& value, std::string_view track_path) {
    if (!value.is_object()) Fail(track_path, "every clip must be an object");
    Clip clip;
    clip.id = ReadString(Required(value, "id", track_path), track_path, "clip id");
    const std::string_view path = clip.id;
    const int type = ReadEnum(Required(value, "type", path), kCommandTypeNames, path, "clip type");
    clip.start = ReadInt(Required(value, "start", path), path, "clip start");
    if (const Json* end = Member(value, "end")) {
        if (!end->is_null()) clip.end = ReadInt(*end, path, "clip end");
    }
    if (const Json* gate = Member(value, "when")) {
        if (!gate->is_null()) clip.when = ReadGate(*gate, path);
    }
    if (const Json* label = Member(value, "label")) clip.label = ReadString(*label, path, "label");
    if (const Json* muted = Member(value, "muted")) clip.muted = ReadBool(*muted, path, "muted");
    clip.command = ReadCommand((CommandType)type, Member(value, "params"), path, clip.params_extra);
    if (const Json* keys = Member(value, "keys")) {
        if (!keys->is_array()) Fail(path, "keys must be an array");
        for (const Json& key : *keys)
            clip.keys.push_back(ReadKey(key, (CommandType)type, path));
    }
    clip.extra = CollectExtra(value, kClipKeys);
    return clip;
}

Json WriteClip(const Clip& clip) {
    const CommandType type = TypeOf(clip.command);
    Json out = Json::object();
    out["id"] = clip.id;
    out["type"] = std::string(NameForIndex(kCommandTypeNames, (int)type));
    out["start"] = clip.start;
    if (!IsEvent(type)) {
        if (clip.end.has_value()) {
            out["end"] = *clip.end;
        } else {
            out["end"] = nullptr;
        }
    }
    if (clip.when.has_value()) out["when"] = WriteGate(*clip.when);
    if (!clip.label.empty()) out["label"] = clip.label;
    if (clip.muted) out["muted"] = clip.muted;
    const Json params = WriteCommand(clip.command, clip.params_extra);
    if (!params.empty()) out["params"] = params;
    if (!clip.keys.empty()) {
        Json keys = Json::array();
        for (std::size_t i = 0; i < clip.keys.size(); ++i) {
            keys.push_back(WriteKey(clip.keys[i], type, i + 1 == clip.keys.size()));
        }
        out["keys"] = keys;
    }
    WriteExtra(out, clip.extra);
    return out;
}

constexpr std::array<std::string_view, 9> kTrackKeys = {
    "id", "name", "kind", "target", "muted", "solo", "locked", "color", "clips"};

Track ReadTrack(const Json& value, std::string_view path) {
    if (!value.is_object()) Fail(path, "every track must be an object");
    Track track;
    track.id = ReadString(Required(value, "id", path), path, "track id");
    track.kind = (TrackKind)ReadEnum(Required(value, "kind", track.id), kTrackKindNames, track.id,
                                     "track kind");
    if (const Json* target = Member(value, "target")) {
        track.target = ReadString(*target, track.id, "track target");
    }
    if (const Json* name = Member(value, "name")) {
        track.name = ReadString(*name, track.id, "track name");
    } else if (HasTarget(track.kind)) {
        track.name = track.target;
    } else {
        track.name = std::string(NameForIndex(kTrackKindNames, (int)track.kind));
    }
    if (const Json* muted = Member(value, "muted")) {
        track.muted = ReadBool(*muted, track.id, "track muted");
    }
    if (const Json* solo = Member(value, "solo")) {
        track.solo = ReadBool(*solo, track.id, "track solo");
    }
    if (const Json* locked = Member(value, "locked")) {
        track.locked = ReadBool(*locked, track.id, "track locked");
    }
    if (const Json* color = Member(value, "color")) {
        if (!color->is_null()) track.color = ReadString(*color, track.id, "track color");
    }
    for (const Json& clip : Required(value, "clips", track.id)) {
        track.clips.push_back(ReadClip(clip, track.id));
    }
    track.extra = CollectExtra(value, kTrackKeys);
    return track;
}

Json WriteTrack(const Track& track) {
    Json out = Json::object();
    out["id"] = track.id;
    out["name"] = track.name;
    out["kind"] = std::string(NameForIndex(kTrackKindNames, (int)track.kind));
    if (HasTarget(track.kind)) out["target"] = track.target;
    if (track.muted) out["muted"] = track.muted;
    if (track.solo) out["solo"] = track.solo;
    if (track.locked) out["locked"] = track.locked;
    if (!track.color.empty()) out["color"] = track.color;
    Json clips = Json::array();
    for (const Clip& clip : track.clips)
        clips.push_back(WriteClip(clip));
    out["clips"] = clips;
    WriteExtra(out, track.extra);
    return out;
}

Json WriteOverride(const OverrideValue& value) {
    if (const auto* held = std::get_if<bool>(&value)) return *held;
    if (const auto* held = std::get_if<double>(&value)) return *held;
    if (const auto* held = std::get_if<std::string>(&value)) return *held;
    return WriteArray(std::get<Vec3>(value));
}

OptionSpec ReadOption(const Json& value, std::string_view path) {
    if (!value.is_object()) Fail(path, "every option must be an object");
    OptionSpec option;
    option.id = ReadString(Required(value, "id", path), path, "option id");
    option.label = ReadString(Required(value, "label", path), option.id, "option label");
    option.default_choice =
        ReadInt(Required(value, "default_choice", option.id), option.id, "default_choice");
    const Json& transition = Required(value, "transition", option.id);
    option.transition.frames =
        ReadInt(Required(transition, "frames", option.id), option.id, "transition.frames");
    option.transition.step =
        ReadInt(Required(transition, "step", option.id), option.id, "transition.step");
    option.transition.ease = (Ease)ReadEnum(Required(transition, "ease", option.id), kEaseNames,
                                            option.id, "transition.ease");
    option.transition.spin_kick =
        ReadNumber(Required(transition, "spin_kick", option.id), option.id, "transition.spin_kick");
    for (const Json& entry : Required(value, "choices", option.id)) {
        ChoiceSpec choice;
        choice.label = ReadString(Required(entry, "label", option.id), option.id, "choice label");
        for (const auto& item : Required(entry, "values", option.id).items()) {
            choice.values.push_back(ChoiceValue{
                .id = item.key(), .value = ReadOverride(item.value(), option.id, item.key())});
        }
        option.choices.push_back(choice);
    }
    return option;
}

Json WriteOption(const OptionSpec& option) {
    Json out = Json::object();
    out["id"] = option.id;
    out["label"] = option.label;
    out["default_choice"] = option.default_choice;
    Json transition = Json::object();
    transition["frames"] = option.transition.frames;
    transition["step"] = option.transition.step;
    transition["ease"] = std::string(NameForIndex(kEaseNames, (int)option.transition.ease));
    transition["spin_kick"] = option.transition.spin_kick;
    out["transition"] = transition;
    Json choices = Json::array();
    for (const ChoiceSpec& choice : option.choices) {
        Json entry = Json::object();
        entry["label"] = choice.label;
        Json values = Json::object();
        for (const ChoiceValue& value : choice.values)
            values[value.id] = WriteOverride(value.value);
        entry["values"] = values;
        choices.push_back(entry);
    }
    out["choices"] = choices;
    return out;
}

RenderSpec ReadRender(const Json& value, std::string_view path) {
    RenderSpec render;
    render.width = ReadInt(Required(value, "width", path), path, "render.width");
    render.height = ReadInt(Required(value, "height", path), path, "render.height");
    render.opaque = ReadBool(Required(value, "opaque", path), path, "render.opaque");
    render.shading =
        (Shading)ReadEnum(Required(value, "shading", path), kShadingNames, path, "render.shading");
    render.sprite_split_priority = ReadInt(Required(value, "sprite_split_priority", path), path,
                                           "render.sprite_split_priority");
    return render;
}

Json WriteRender(const RenderSpec& render) {
    Json out = Json::object();
    out["width"] = render.width;
    out["height"] = render.height;
    out["opaque"] = render.opaque;
    out["shading"] = std::string(NameForIndex(kShadingNames, (int)render.shading));
    out["sprite_split_priority"] = render.sprite_split_priority;
    return out;
}

CameraSpec ReadCamera(const Json& value, std::string_view path) {
    CameraSpec camera;
    camera.eye = ReadArray<3>(Required(value, "eye", path), path, "camera.eye");
    camera.at = ReadArray<3>(Required(value, "at", path), path, "camera.at");
    camera.up = ReadArray<3>(Required(value, "up", path), path, "camera.up");
    camera.fov_y = ReadNumber(Required(value, "fov_y", path), path, "camera.fov_y");
    camera.near_z = ReadNumber(Required(value, "near_z", path), path, "camera.near_z");
    camera.far_z = ReadNumber(Required(value, "far_z", path), path, "camera.far_z");
    const Json& aspect = Required(value, "aspect", path);
    if (aspect.is_string()) {
        if (aspect.get<std::string>() != "auto")
            Fail(path, "camera.aspect must be a number or auto");
        camera.aspect = AspectSpec{.automatic = true, .value = 0.0};
    } else {
        camera.aspect =
            AspectSpec{.automatic = false, .value = ReadNumber(aspect, path, "camera.aspect")};
    }
    return camera;
}

Json WriteCamera(const CameraSpec& camera) {
    Json out = Json::object();
    out["eye"] = WriteArray(camera.eye);
    out["at"] = WriteArray(camera.at);
    out["up"] = WriteArray(camera.up);
    out["fov_y"] = camera.fov_y;
    out["near_z"] = camera.near_z;
    out["far_z"] = camera.far_z;
    if (camera.aspect.automatic) {
        out["aspect"] = "auto";
    } else {
        out["aspect"] = camera.aspect.value;
    }
    return out;
}

void ReadLists(const Json& root, Document& doc) {
    const std::string_view path = doc.id;
    for (const Json& light : Required(root, "lights", path)) {
        doc.lights.push_back(
            LightSpec{.direction = ReadArray<3>(Required(light, "direction", path), path, "light"),
                      .diffuse = ReadArray<3>(Required(light, "diffuse", path), path, "light"),
                      .specular = ReadArray<3>(Required(light, "specular", path), path, "light")});
    }
    for (const auto& item : Required(root, "assets", path).items()) {
        doc.assets.push_back(
            Asset{.id = item.key(),
                  .kind = (AssetKind)ReadEnum(Required(item.value(), "kind", path), kAssetKindNames,
                                              path, "asset kind"),
                  .dir = ReadString(Required(item.value(), "dir", path), path, "asset dir")});
    }
    for (const Json& option : Required(root, "options", path)) {
        doc.options.push_back(ReadOption(option, path));
    }
    for (const Json& marker : Required(root, "markers", path)) {
        doc.markers.push_back(
            Marker{.frame = ReadInt(Required(marker, "frame", path), path, "marker frame"),
                   .label = ReadString(Required(marker, "label", path), path, "marker label")});
    }
    for (const Json& track : Required(root, "tracks", path)) {
        doc.tracks.push_back(ReadTrack(track, path));
    }
}

void WriteLists(const Document& doc, Json& root) {
    Json lights = Json::array();
    for (const LightSpec& light : doc.lights) {
        Json entry = Json::object();
        entry["direction"] = WriteArray(light.direction);
        entry["diffuse"] = WriteArray(light.diffuse);
        entry["specular"] = WriteArray(light.specular);
        lights.push_back(entry);
    }
    root["lights"] = lights;
    Json assets = Json::object();
    for (const Asset& asset : doc.assets) {
        Json entry = Json::object();
        entry["kind"] = std::string(NameForIndex(kAssetKindNames, (int)asset.kind));
        entry["dir"] = asset.dir;
        assets[asset.id] = entry;
    }
    root["assets"] = assets;
    Json options = Json::array();
    for (const OptionSpec& option : doc.options)
        options.push_back(WriteOption(option));
    root["options"] = options;
    root["rng_seed"] = doc.rng_seed;
    Json markers = Json::array();
    for (const Marker& marker : doc.markers) {
        Json entry = Json::object();
        entry["frame"] = marker.frame;
        entry["label"] = marker.label;
        markers.push_back(entry);
    }
    root["markers"] = markers;
    Json tracks = Json::array();
    for (const Track& track : doc.tracks)
        tracks.push_back(WriteTrack(track));
    root["tracks"] = tracks;
}

constexpr std::array<std::string_view, 16> kDocumentKeys = {
    "schema", "version", "id",     "name",    "build",    "fps",     "length", "render",
    "camera", "lights",  "assets", "options", "rng_seed", "markers", "tracks", "notes"};

Document ReadDocument(const Json& root) {
    if (!root.is_object()) Fail("", "the document must be a JSON object");
    Document doc;
    doc.schema = ReadString(Required(root, "schema", ""), "", "schema");
    if (doc.schema != kSchemaId) {
        Fail("", "schema \"" + doc.schema + "\" is not " + std::string(kSchemaId));
    }
    doc.version = ReadInt(Required(root, "version", ""), "", "version");
    if (doc.version != kSchemaVersion) {
        Fail("", "version " + std::to_string(doc.version) +
                     " cannot be read by this build, which "
                     "understands version 1");
    }
    doc.id = ReadString(Required(root, "id", ""), "", "id");
    doc.name = ReadString(Required(root, "name", doc.id), doc.id, "name");
    doc.build = ReadString(Required(root, "build", doc.id), doc.id, "build");
    doc.fps = ReadInt(Required(root, "fps", doc.id), doc.id, "fps");
    const Json& length = Required(root, "length", doc.id);
    if (length.is_string()) {
        if (length.get<std::string>() != "auto") Fail(doc.id, "length must be a number or auto");
    } else {
        doc.length = ReadInt(length, doc.id, "length");
    }
    doc.render = ReadRender(Required(root, "render", doc.id), doc.id);
    doc.camera = ReadCamera(Required(root, "camera", doc.id), doc.id);
    doc.rng_seed = ReadInt(Required(root, "rng_seed", doc.id), doc.id, "rng_seed");
    ReadLists(root, doc);
    if (const Json* notes = Member(root, "notes")) doc.notes = ReadString(*notes, doc.id, "notes");
    doc.extra = CollectExtra(root, kDocumentKeys);
    return doc;
}

ParseError SyntaxError(const nlohmann::json::parse_error& failure, std::string_view text) {
    ParseError error;
    error.message = failure.what();
    const std::size_t byte = failure.byte == 0 ? 0 : failure.byte - 1;
    const std::size_t upto = byte < text.size() ? byte : text.size();
    error.line = 1;
    std::size_t line_start = 0;
    for (std::size_t i = 0; i < upto; ++i) {
        if (text[i] == '\n') {
            ++error.line;
            line_start = i + 1;
        }
    }
    error.column = (int)(upto - line_start) + 1;
    return error;
}

}

Loaded Load(std::string_view text) {
    Json root;
    try {
        root = Json::parse(text);
    } catch (const nlohmann::json::parse_error& failure) {
        return Support::Unexpected(SyntaxError(failure, text));
    }
    try {
        return ReadDocument(root);
    } catch (const ParseFail& failure) {
        return Support::Unexpected(failure.error);
    }
}

std::string Save(const Document& doc) {
    Json root = Json::object();
    root["schema"] = doc.schema;
    root["version"] = doc.version;
    root["id"] = doc.id;
    root["name"] = doc.name;
    root["build"] = doc.build;
    root["fps"] = doc.fps;
    if (doc.length.has_value()) {
        root["length"] = *doc.length;
    } else {
        root["length"] = "auto";
    }
    root["render"] = WriteRender(doc.render);
    root["camera"] = WriteCamera(doc.camera);
    WriteLists(doc, root);
    if (!doc.notes.empty()) root["notes"] = doc.notes;
    WriteExtra(root, doc.extra);
    return root.dump(2) + "\n";
}

}
