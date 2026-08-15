#include "preset/doc/preset_validate.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_fields.h"
#include "preset/preset_params.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Preset::Doc {

namespace {

struct Report {
    std::vector<Problem> problems;

    void Add(Severity severity, std::string_view path, std::string message) {
        problems.push_back(Problem{
            .severity = severity, .path = std::string(path), .message = std::move(message)});
    }
    void Error(std::string_view path, std::string message) {
        Add(Severity::Error, path, std::move(message));
    }
    void Warn(std::string_view path, std::string message) {
        Add(Severity::Warning, path, std::move(message));
    }
};

constexpr std::array<std::string_view, 7> kModelParams = {
    "blend_mode", "alpha", "anim_speed", "position", "rotation", "scale", "spin_per_frame"};
constexpr std::array<std::string_view, 6> kSpriteParams = {"x",     "y",     "alpha",
                                                           "scale", "blend", "priority"};
constexpr std::array<std::string_view, 7> kCameraParams = {"eye",    "at",    "up",    "fov_y",
                                                           "near_z", "far_z", "aspect"};
constexpr std::array<std::string_view, 3> kLightParams = {"direction", "diffuse", "specular"};

bool Contains(std::span<const std::string_view> names, std::string_view name) {
    return std::ranges::find(names, name) != names.end();
}

bool Bracketed(std::string_view id, std::string_view prefix,
               std::span<const std::string_view> fields, bool numeric_name) {
    if (!id.starts_with(prefix)) return false;
    const std::size_t close = id.find("].", prefix.size());
    if (close == std::string_view::npos || close == prefix.size()) return false;
    const std::string_view name = id.substr(prefix.size(), close - prefix.size());
    if (numeric_name && name.find_first_not_of("0123456789") != std::string_view::npos)
        return false;
    return Contains(fields, id.substr(close + 2));
}

bool IsParamId(std::string_view id) {
    if (id == "shading" || id == "sprite_split_priority") return true;
    if (id.starts_with("camera.")) return Contains(kCameraParams, id.substr(7));
    if (Bracketed(id, "model[", kModelParams, false)) return true;
    if (Bracketed(id, "sprite[", kSpriteParams, false)) return true;
    return Bracketed(id, "light[", kLightParams, true);
}

int ClipEnd(const Clip& clip, const Document& doc) {
    if (IsEvent(TypeOf(clip.command))) return clip.start + 1;
    if (clip.end.has_value()) return *clip.end;
    if (doc.length.has_value()) return *doc.length;
    return std::numeric_limits<int>::max();
}

std::optional<int> Duration(const Clip& clip, const Document& doc) {
    if (IsEvent(TypeOf(clip.command))) return 0;
    if (clip.end.has_value()) return *clip.end - clip.start;
    if (doc.length.has_value()) return *doc.length - clip.start;
    return std::nullopt;
}

bool Overlaps(const Clip& a, const Clip& b, const Document& doc) {
    return a.start < ClipEnd(b, doc) && b.start < ClipEnd(a, doc);
}

bool Disjoint(std::span<const std::string> a, std::span<const std::string> b) {
    return std::ranges::none_of(
        a, [b](const std::string& value) { return std::ranges::find(b, value) != b.end(); });
}

bool Subset(std::span<const std::string> a, std::span<const std::string> b) {
    return std::ranges::all_of(
        a, [b](const std::string& value) { return std::ranges::find(b, value) != b.end(); });
}

bool ExclusiveGates(const std::optional<Gate>& first, const std::optional<Gate>& second) {
    if (!first.has_value() || !second.has_value()) return false;
    if (first->option != second->option) return false;
    const bool first_not = first->kind == GateKind::Not;
    const bool second_not = second->kind == GateKind::Not;
    if (!first_not && !second_not) return Disjoint(first->choices, second->choices);
    if (first_not && !second_not) return Subset(second->choices, first->choices);
    if (!first_not && second_not) return Subset(first->choices, second->choices);
    return false;
}

std::string TypeName(const Clip& clip) {
    return std::string(NameForIndex(kCommandTypeNames, (int)TypeOf(clip.command)));
}

void CheckHeader(const Document& doc, std::span<const std::string_view> builtin_ids, Report& out) {
    if (doc.schema != kSchemaId) {
        out.Error(doc.id, "schema \"" + doc.schema + "\" is not " + std::string(kSchemaId));
    }
    if (doc.version != kSchemaVersion) {
        out.Error(doc.id, "version " + std::to_string(doc.version) + " is not the schema version");
    }
    if (doc.fps <= 0) out.Error(doc.id, "fps must be positive");
    if (doc.length.has_value() && *doc.length <= 0) out.Error(doc.id, "length must be positive");
    if (Contains(builtin_ids, doc.id)) {
        out.Error(doc.id, "id \"" + doc.id + "\" is already a built-in preset id for this build");
    }
}

void CheckIds(const Document& doc, Report& out) {
    std::vector<std::string_view> track_ids;
    std::vector<std::string_view> clip_ids;
    for (const Track& track : doc.tracks) {
        if (Contains(track_ids, track.id)) out.Error(track.id, "duplicate track id");
        track_ids.push_back(track.id);
        for (const Clip& clip : track.clips) {
            if (Contains(clip_ids, clip.id)) out.Error(clip.id, "duplicate clip id");
            clip_ids.push_back(clip.id);
        }
    }
    std::vector<std::string_view> asset_ids;
    for (const Asset& asset : doc.assets) {
        if (Contains(asset_ids, asset.id)) out.Error(doc.id, "duplicate asset id " + asset.id);
        asset_ids.push_back(asset.id);
    }
    std::vector<std::string_view> option_ids;
    for (const OptionSpec& option : doc.options) {
        if (Contains(option_ids, option.id)) out.Error(doc.id, "duplicate option id " + option.id);
        option_ids.push_back(option.id);
    }
    for (std::size_t i = 1; i < doc.markers.size(); ++i) {
        if (doc.markers[i].frame <= doc.markers[i - 1].frame) {
            out.Error(doc.id, "markers must be sorted by frame with one marker per frame");
            break;
        }
    }
}

void CheckOptions(const Document& doc, Report& out) {
    for (const OptionSpec& option : doc.options) {
        if (option.transition.ease == Ease::SineDeg || option.transition.ease == Ease::Bezier) {
            out.Error(option.id, "transition ease must be hold, linear, ease_in, ease_out or "
                                 "ease_in_out");
        }
        if (option.default_choice < 0 ||
            (std::size_t)option.default_choice >= option.choices.size()) {
            out.Error(option.id, "default_choice is outside the choice list");
        }
        std::vector<std::string_view> labels;
        for (const ChoiceSpec& choice : option.choices) {
            if (Contains(labels, choice.label)) {
                out.Error(option.id, "duplicate choice label " + choice.label);
            }
            labels.push_back(choice.label);
            for (const ChoiceValue& value : choice.values) {
                if (!IsParamId(value.id)) {
                    out.Error(option.id,
                              "choice value \"" + value.id + "\" is not a schema parameter id");
                }
            }
        }
    }
}

void CheckRange(const FieldDesc& field, const ParamValue& value, std::string_view path,
                Report& out) {
    if (field.range.soft || field.range.max <= field.range.min) return;
    std::vector<double> numbers;
    if (const auto* held = std::get_if<int>(&value)) numbers.push_back((double)*held);
    if (const auto* held = std::get_if<double>(&value)) numbers.push_back(*held);
    if (const auto* held = std::get_if<Vec2>(&value)) numbers.assign(held->begin(), held->end());
    if (const auto* held = std::get_if<Vec3>(&value)) numbers.assign(held->begin(), held->end());
    std::string unit;
    if (!field.unit.empty()) {
        unit += " ";
        unit += field.unit;
    }
    for (const double number : numbers) {
        if (number < field.range.min || number > field.range.max) {
            std::string message(field.id);
            message += " ";
            message += std::to_string(number);
            message += unit;
            message += " is outside the range ";
            message += std::to_string(field.range.min);
            message += " to ";
            message += std::to_string(field.range.max);
            message += unit;
            out.Error(path, message);
            return;
        }
    }
}

void CheckParams(const Document& doc, const Clip& clip, Report& out) {
    const CommandType type = TypeOf(clip.command);
    for (const FieldDesc& field : FieldsFor(type)) {
        const ParamValue value = field.get(clip.command);
        const auto* text = std::get_if<std::string>(&value);
        if (field.required && (text == nullptr || text->empty())) {
            out.Error(clip.id, "required parameter " + std::string(field.id) + " is missing (" +
                                   std::string(field.help) + ")");
        }
        if (field.kind == FieldKind::Enum && std::holds_alternative<int>(value)) {
            const int index = std::get<int>(value);
            if (index < 0 || (std::size_t)index >= field.enum_names.size()) {
                out.Error(clip.id, std::string(field.id) + " holds no name of its enum");
            }
        }
        CheckRange(field, value, clip.id, out);
        if (field.id != "asset" || text == nullptr || text->empty()) continue;
        const bool known =
            std::ranges::any_of(doc.assets, [&](const Asset& asset) { return asset.id == *text; });
        if (!known) out.Error(clip.id, "asset \"" + *text + "\" is not in the assets table");
    }
    if (type != CommandType::ParamOverride) return;
    const std::string& id = std::get<ParamOverrideCmd>(clip.command).id;
    if (!IsParamId(id)) out.Error(clip.id, "\"" + id + "\" is not a schema parameter id");
}

void CheckKeys(const Document& doc, const Clip& clip, Report& out) {
    const CommandType type = TypeOf(clip.command);
    const std::optional<int> duration = Duration(clip, doc);
    const std::span<const FieldDesc> fields = KeyFieldsFor(type);
    for (const Key& key : clip.keys) {
        if (key.at < 0) {
            out.Error(clip.id, "key at " + std::to_string(key.at) + " is before the clip start");
        } else if (duration.has_value() && key.at > *duration) {
            out.Warn(clip.id, "key at " + std::to_string(key.at) +
                                  " is never reached: the clip is " + std::to_string(*duration) +
                                  " frames long");
        }
        if (key.rate_deg.has_value() && key.ease != Ease::SineDeg) {
            out.Error(clip.id, "rate_deg only belongs on a sine_deg key");
        }
        if (key.cp.has_value() && key.ease != Ease::Bezier) {
            out.Error(clip.id, "cp only belongs on a bezier key");
        }
        for (const KeyValue& value : key.values) {
            const FieldDesc* field = FindField(fields, value.id);
            if (field == nullptr && type == CommandType::SpriteAnimate && value.id == "blend") {
                out.Warn(clip.id, "blend is chosen per node from the package, so the key is "
                                  "ignored");
                continue;
            }
            if (field == nullptr) {
                out.Error(clip.id,
                          "key value \"" + value.id + "\" is not a parameter of " + TypeName(clip));
                continue;
            }
            if (!field->tweenable) {
                out.Error(clip.id, "key value \"" + value.id + "\" is not tweenable");
            }
        }
    }
}

void CheckGate(const Document& doc, const Clip& clip, Report& out) {
    if (!clip.when.has_value()) return;
    const Gate& gate = *clip.when;
    const auto found = std::ranges::find_if(
        doc.options, [&](const OptionSpec& option) { return option.id == gate.option; });
    if (found == doc.options.end()) {
        out.Error(clip.id, "when names the unknown option \"" + gate.option + "\"");
        return;
    }
    for (const std::string& label : gate.choices) {
        const bool known = std::ranges::any_of(
            found->choices, [&](const ChoiceSpec& choice) { return choice.label == label; });
        if (!known) {
            out.Error(clip.id,
                      "when names the unknown choice \"" + label + "\" of option " + gate.option);
        }
    }
}

void CheckClip(const Document& doc, const Track& track, const Clip& clip, Report& out) {
    const CommandType type = TypeOf(clip.command);
    if (TraitsFor(type).kind != track.kind) {
        out.Error(clip.id,
                  TypeName(clip) + " clips only belong on a " +
                      std::string(NameForIndex(kTrackKindNames, (int)TraitsFor(type).kind)) +
                      " track, not on a " +
                      std::string(NameForIndex(kTrackKindNames, (int)track.kind)) + " track");
    }
    if (clip.start < 0) out.Error(clip.id, "start is before frame 0");
    if (IsEvent(type)) {
        if (clip.end.has_value() && *clip.end != clip.start) {
            out.Error(clip.id, "an event clip carries no end");
        }
    } else if (clip.end.has_value() && *clip.end <= clip.start) {
        out.Error(clip.id, "end must be after start or null for an open ended clip");
    }
    CheckParams(doc, clip, out);
    CheckKeys(doc, clip, out);
    CheckGate(doc, clip, out);
}

struct Primary {
    const Track* track = nullptr;
    const Clip* clip = nullptr;
    std::string key;
};

std::string PrimaryKey(const Track& track, const Clip& clip) {
    const Family family = TraitsFor(TypeOf(clip.command)).family;
    if (family == Family::Sprite || family == Family::ModelDraw) return track.target;
    if (family == Family::Light) {
        return track.id + "#" + std::to_string(std::get<LightSet>(clip.command).index);
    }
    return track.id;
}

std::vector<Primary> CollectPrimaries(const Document& doc, Family family) {
    std::vector<Primary> primaries;
    for (const Track& track : doc.tracks) {
        for (const Clip& clip : track.clips) {
            if (TraitsFor(TypeOf(clip.command)).family != family) continue;
            primaries.push_back(
                Primary{.track = &track, .clip = &clip, .key = PrimaryKey(track, clip)});
        }
    }
    return primaries;
}

struct PairLog {
    std::vector<std::string> seen;

    bool FirstTime(const Primary& first, const Primary& second) {
        std::string pair = first.track->id + "\n" + second.track->id + "\n" + second.key;
        if (std::ranges::find(seen, pair) != seen.end()) return false;
        seen.push_back(std::move(pair));
        return true;
    }
};

void CheckTrackPair(Family family, const Primary& first, const Primary& second, PairLog& log,
                    Report& out) {
    if (family != Family::Sprite && family != Family::ModelDraw) return;
    if (first.track == second.track) return;
    if (!log.FirstTime(first, second)) return;
    out.Error(second.track->id, "track \"" + second.track->id +
                                    "\" holds a second set of primary clips for target \"" +
                                    second.key + "\"");
}

void CheckClipPair(const Document& doc, const Primary& first, const Primary& second, Report& out) {
    if (!Overlaps(*first.clip, *second.clip, doc)) return;
    if (ExclusiveGates(first.clip->when, second.clip->when)) return;
    out.Error(second.clip->id, "clip \"" + second.clip->id + "\" overlaps \"" + first.clip->id +
                                   "\", which is a primary of the same family");
}

void CheckOverlaps(const Document& doc, Report& out) {
    for (int raw = (int)Family::Sprite; raw <= (int)Family::RngSeed; ++raw) {
        const std::vector<Primary> primaries = CollectPrimaries(doc, (Family)raw);
        PairLog log;
        for (std::size_t i = 0; i < primaries.size(); ++i) {
            for (std::size_t j = i + 1; j < primaries.size(); ++j) {
                if (primaries[i].key != primaries[j].key) continue;
                CheckTrackPair((Family)raw, primaries[i], primaries[j], log, out);
                CheckClipPair(doc, primaries[i], primaries[j], out);
            }
        }
    }
}

bool CoveredByDraw(const Document& doc, const Clip& clip, std::string_view target) {
    for (const Track& track : doc.tracks) {
        if (track.kind != TrackKind::Model || track.target != target) continue;
        for (const Clip& other : track.clips) {
            if (TypeOf(other.command) != CommandType::ModelDraw) continue;
            if (Overlaps(clip, other, doc)) return true;
        }
    }
    return false;
}

std::size_t DrawTrackIndex(const Document& doc, std::string_view target) {
    for (std::size_t i = 0; i < doc.tracks.size(); ++i) {
        if (doc.tracks[i].kind != TrackKind::Model || doc.tracks[i].target != target) continue;
        for (const Clip& clip : doc.tracks[i].clips) {
            if (TypeOf(clip.command) == CommandType::ModelDraw) return i;
        }
    }
    return doc.tracks.size();
}

bool ScrollCovered(const Track& track, const Clip& clip, const Document& doc) {
    return std::ranges::any_of(track.clips, [&](const Clip& other) {
        const CommandType type = TypeOf(other.command);
        const bool sprite_clip =
            type == CommandType::SpriteDraw || type == CommandType::SpriteAnimate;
        return sprite_clip && Overlaps(clip, other, doc);
    });
}

void CheckModifiers(const Document& doc, Report& out) {
    for (std::size_t index = 0; index < doc.tracks.size(); ++index) {
        const Track& track = doc.tracks[index];
        for (const Clip& clip : track.clips) {
            const CommandType type = TypeOf(clip.command);
            if (type == CommandType::SpriteScroll && !ScrollCovered(track, clip, doc)) {
                out.Error(clip.id, "sprite.scroll has no sprite clip under it on the same track");
            }
            if (type != CommandType::ModelTween && type != CommandType::ModelMotion) continue;
            if (!CoveredByDraw(doc, clip, track.target)) {
                out.Warn(clip.id, "no model.draw of target \"" + track.target +
                                      "\" runs under this clip, so it changes nothing");
                continue;
            }
            if (index < DrawTrackIndex(doc, track.target)) {
                out.Warn(track.id, "modifier track \"" + track.id + "\" for target \"" +
                                       track.target + "\" is evaluated before the draw track");
            }
        }
    }
}

}

std::vector<Problem> Validate(const Document& doc, std::span<const std::string_view> builtin_ids) {
    Report out;
    CheckHeader(doc, builtin_ids, out);
    CheckIds(doc, out);
    CheckOptions(doc, out);
    for (const Track& track : doc.tracks) {
        for (const Clip& clip : track.clips)
            CheckClip(doc, track, clip, out);
    }
    CheckOverlaps(doc, out);
    CheckModifiers(doc, out);
    return out.problems;
}

}
