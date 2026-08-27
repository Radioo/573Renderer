#include "editor/tween_edits.h"

#include "editor/timeline_edits.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_fields.h"
#include "preset/eval/eval_resolve.h"
#include "preset/eval/eval_scene.h"
#include "preset/eval/eval_tween.h"
#include "preset/eval/frame_state.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Editor {

namespace Doc = Preset::Doc;

namespace {

constexpr double kDefaultRateDeg = 3.0;
constexpr std::array<double, 4> kDefaultBezier = {0.42, 0.0, 0.58, 1.0};

Doc::Clip* MutableClip(Doc::Document& document, std::string_view clip_id) {
    const ClipRef ref = FindClip(document, clip_id);
    if (!ref.Valid()) return nullptr;
    return &document.tracks[(std::size_t)ref.track].clips[(std::size_t)ref.clip];
}

Doc::Key* MutableKey(Doc::Document& document, std::string_view clip_id, int index) {
    Doc::Clip* clip = MutableClip(document, clip_id);
    if (clip == nullptr) return nullptr;
    if (index < 0 || std::cmp_greater_equal(index, clip->keys.size())) return nullptr;
    return &clip->keys[(std::size_t)index];
}

const Doc::Track* TrackOfClip(const Doc::Document& document, std::string_view clip_id) {
    const ClipRef ref = FindClip(document, clip_id);
    if (!ref.Valid()) return nullptr;
    return &document.tracks[(std::size_t)ref.track];
}

std::string TargetIdFor(const Doc::Track& track, const Doc::Clip& clip, std::string_view field) {
    switch (track.kind) {
    case Doc::TrackKind::Model:
        return "model[" + track.target + "]." + std::string(field);
    case Doc::TrackKind::Sprite:
        return "sprite[" + track.target + "]." + std::string(field);
    case Doc::TrackKind::Camera:
        return "camera." + std::string(field);
    case Doc::TrackKind::Light: {
        const auto* light = std::get_if<Doc::LightSet>(&clip.command);
        if (light == nullptr) return {};
        return "light[" + std::to_string(light->index) + "]." + std::string(field);
    }
    default:
        return {};
    }
}

Doc::ParamValue FromTween(const Preset::Eval::TweenValue& value) {
    switch (value.kind) {
    case Preset::Eval::TweenValue::Kind::Vector:
        return Doc::Vec3{(double)value.vector[0], (double)value.vector[1], (double)value.vector[2]};
    case Preset::Eval::TweenValue::Kind::Integer:
        return value.integer;
    case Preset::Eval::TweenValue::Kind::Scalar:
    default:
        return (double)value.scalar;
    }
}

Preset::Eval::FrameState StateAt(const Doc::Document& document, int frame) {
    const std::vector<int> choices;
    const Preset::Eval::ResolveInput input{
        .document = &document, .choices = &choices, .tweens = true};
    return Preset::Eval::ResolveFrame(input, frame);
}

std::optional<Doc::ParamValue> FallbackValue(const Doc::Clip& clip, std::string_view field_id) {
    const Doc::CommandType type = Doc::TypeOf(clip.command);
    const std::span<const Doc::FieldDesc> fields = Doc::KeyFieldsFor(type);
    const Doc::FieldDesc* desc = Doc::FindField(fields, field_id);
    if (desc == nullptr || desc->get == nullptr) return {};
    const Doc::Command& source =
        (Doc::TypeOf(clip.command) == type) ? clip.command : Doc::DefaultCommand(type);
    Doc::ParamValue value = desc->get(source);
    if (std::holds_alternative<std::monostate>(value)) return {};
    return value;
}

std::vector<std::string> KeyedFields(const Doc::Clip& clip) {
    std::vector<std::string> out;
    for (const Doc::Key& key : clip.keys) {
        for (const Doc::KeyValue& value : key.values) {
            if (std::ranges::find(out, value.id) == out.end()) out.push_back(value.id);
        }
    }
    return out;
}

const Preset::Eval::ModelSlot* SlotFor(const Preset::Eval::FrameState& state,
                                       const std::string& name) {
    for (const Preset::Eval::ModelSlot& slot : state.models) {
        if (slot.name == name) return &slot;
    }
    return nullptr;
}

Doc::Key PoseKey(const Preset::Eval::ModelSlot& slot) {
    Doc::Key key;
    key.at = 0;
    key.ease = Doc::Ease::Linear;
    key.values.push_back(
        Doc::KeyValue{.id = "position",
                      .value = Doc::Vec3{slot.position[0], slot.position[1], slot.position[2]}});
    key.values.push_back(
        Doc::KeyValue{.id = "rotation",
                      .value = Doc::Vec3{slot.rotation[0], slot.rotation[1], slot.rotation[2]}});
    key.values.push_back(Doc::KeyValue{
        .id = "scale", .value = Doc::Vec3{slot.scale[0], slot.scale[1], slot.scale[2]}});
    key.values.push_back(Doc::KeyValue{.id = "alpha", .value = (double)slot.alpha});
    key.values.push_back(Doc::KeyValue{.id = "anim_speed", .value = (double)slot.anim_speed});
    return key;
}

Doc::Key ParamKey(const Doc::ModelDraw& draw, int at) {
    Doc::Key key;
    key.at = at;
    key.ease = Doc::Ease::Linear;
    key.values.push_back(Doc::KeyValue{.id = "position", .value = draw.position});
    key.values.push_back(Doc::KeyValue{.id = "rotation", .value = draw.rotation});
    key.values.push_back(Doc::KeyValue{.id = "scale", .value = draw.scale});
    key.values.push_back(Doc::KeyValue{.id = "alpha", .value = draw.alpha});
    key.values.push_back(Doc::KeyValue{.id = "anim_speed", .value = draw.anim_speed});
    return key;
}

std::string UniqueTrackId(const Doc::Document& document, const std::string& base) {
    std::string id = base;
    for (int suffix = 2; suffix < 1000; suffix++) {
        const bool taken = std::ranges::any_of(
            document.tracks, [&id](const Doc::Track& track) { return track.id == id; });
        if (!taken) return id;
        id = base + "_" + std::to_string(suffix);
    }
    return id;
}

bool HoldsOnlyTweens(const Doc::Track& track) {
    return std::ranges::all_of(track.clips, [](const Doc::Clip& clip) {
        return Doc::TypeOf(clip.command) == Doc::CommandType::ModelTween;
    });
}

bool SpanFree(const Doc::Document& document, const Doc::Track& track, int start, int end) {
    const int length = DocumentLength(document);
    return std::ranges::none_of(track.clips, [start, end, length](const Doc::Clip& clip) {
        return clip.start < end && ClipEnd(clip, length) > start;
    });
}

int TweenTrackFor(const Doc::Document& document, int draw_track, int start, int end) {
    const std::string& target = document.tracks[(std::size_t)draw_track].target;
    for (std::size_t i = (std::size_t)draw_track + 1; i < document.tracks.size(); i++) {
        const Doc::Track& track = document.tracks[i];
        if (track.kind != Doc::TrackKind::Model || track.target != target) continue;
        if (!HoldsOnlyTweens(track) || track.locked) continue;
        if (!SpanFree(document, track, start, end)) continue;
        return (int)i;
    }
    return -1;
}

}

const Doc::Clip* ClipById(const Doc::Document& document, std::string_view clip_id) {
    const ClipRef ref = FindClip(document, clip_id);
    if (!ref.Valid()) return nullptr;
    return &document.tracks[(std::size_t)ref.track].clips[(std::size_t)ref.clip];
}

int ClipDuration(const Doc::Document& document, std::string_view clip_id) {
    const Doc::Clip* clip = ClipById(document, clip_id);
    if (clip == nullptr) return 0;
    return ClipEnd(*clip, DocumentLength(document)) - clip->start;
}

const Doc::KeyValue* KeyValueOf(const Doc::Key& key, std::string_view field_id) {
    for (const Doc::KeyValue& value : key.values) {
        if (value.id == field_id) return &value;
    }
    return nullptr;
}

std::vector<TweenField> TweenFields(const Doc::Document& document, std::string_view clip_id) {
    std::vector<TweenField> out;
    const Doc::Clip* clip = ClipById(document, clip_id);
    const Doc::Track* track = TrackOfClip(document, clip_id);
    if (clip == nullptr || track == nullptr) return out;
    for (const Doc::FieldDesc& field : Doc::KeyFieldsFor(Doc::TypeOf(clip->command))) {
        if (!field.tweenable) continue;
        out.push_back(TweenField{.id = std::string(field.id),
                                 .target_id = TargetIdFor(*track, *clip, field.id)});
    }
    return out;
}

std::optional<Doc::ParamValue> ResolvedFieldValue(const Doc::Document& document,
                                                  std::string_view clip_id,
                                                  std::string_view field_id, int frame) {
    const Doc::Clip* clip = ClipById(document, clip_id);
    const Doc::Track* track = TrackOfClip(document, clip_id);
    if (clip == nullptr || track == nullptr) return {};

    const std::string target = TargetIdFor(*track, *clip, field_id);
    if (!target.empty()) {
        Preset::Eval::TweenValue value;
        if (Preset::Eval::ReadTarget(target, StateAt(document, frame), value))
            return FromTween(value);
    }
    return FallbackValue(*clip, field_id);
}

int AddKeyAt(Doc::Document& document, std::string_view clip_id, int at) {
    Doc::Clip* clip = MutableClip(document, clip_id);
    if (clip == nullptr) return -1;
    const int duration = ClipDuration(document, clip_id);
    if (at < 0 || at > duration) return -1;
    for (std::size_t i = 0; i < clip->keys.size(); i++) {
        if (clip->keys[i].at == at) return (int)i;
    }

    std::size_t slot = clip->keys.size();
    for (std::size_t i = 0; i < clip->keys.size(); i++) {
        if (clip->keys[i].at > at) {
            slot = i;
            break;
        }
    }

    Doc::Key key;
    key.at = at;
    if (slot > 0) {
        const Doc::Key& previous = clip->keys[slot - 1];
        key.ease = previous.ease;
        key.rate_deg = previous.rate_deg;
        key.cp = previous.cp;
    }
    const int frame = clip->start + at;
    for (const std::string& field : KeyedFields(*clip)) {
        const std::optional<Doc::ParamValue> value =
            ResolvedFieldValue(document, clip_id, field, frame);
        if (!value.has_value()) continue;
        key.values.push_back(Doc::KeyValue{.id = field, .value = *value});
    }

    clip = MutableClip(document, clip_id);
    clip->keys.insert(clip->keys.begin() + (long long)slot, std::move(key));
    return (int)slot;
}

bool MoveKey(Doc::Document& document, std::string_view clip_id, int index, int at) {
    const int duration = ClipDuration(document, clip_id);
    Doc::Clip* clip = MutableClip(document, clip_id);
    if (clip == nullptr) return false;
    if (index < 0 || std::cmp_greater_equal(index, clip->keys.size())) return false;

    const int low = (index > 0) ? clip->keys[(std::size_t)index - 1].at + 1 : 0;
    const int high = (std::cmp_less(index + 1, clip->keys.size()))
                         ? clip->keys[(std::size_t)index + 1].at - 1
                         : duration;
    if (low > high) return false;
    clip->keys[(std::size_t)index].at = std::clamp(at, low, high);
    return true;
}

bool DeleteKey(Doc::Document& document, std::string_view clip_id, int index) {
    Doc::Clip* clip = MutableClip(document, clip_id);
    if (clip == nullptr) return false;
    if (index < 0 || std::cmp_greater_equal(index, clip->keys.size())) return false;
    clip->keys.erase(clip->keys.begin() + index);
    return true;
}

bool SetKeyEase(Doc::Document& document, std::string_view clip_id, int index, Doc::Ease ease) {
    Doc::Key* key = MutableKey(document, clip_id, index);
    if (key == nullptr) return false;
    key->ease = ease;
    if (ease == Doc::Ease::SineDeg) {
        if (!key->rate_deg.has_value()) key->rate_deg = kDefaultRateDeg;
    } else {
        key->rate_deg.reset();
    }
    if (ease == Doc::Ease::Bezier) {
        if (!key->cp.has_value()) key->cp = kDefaultBezier;
    } else {
        key->cp.reset();
    }
    return true;
}

bool SetKeyRate(Doc::Document& document, std::string_view clip_id, int index, double rate_deg) {
    Doc::Key* key = MutableKey(document, clip_id, index);
    if (key == nullptr || key->ease != Doc::Ease::SineDeg) return false;
    key->rate_deg = rate_deg;
    return true;
}

bool SetKeyBezier(Doc::Document& document, std::string_view clip_id, int index,
                  const std::array<double, 4>& cp) {
    Doc::Key* key = MutableKey(document, clip_id, index);
    if (key == nullptr || key->ease != Doc::Ease::Bezier) return false;
    key->cp = std::array<double, 4>{std::clamp(cp[0], 0.0, 1.0), cp[1], std::clamp(cp[2], 0.0, 1.0),
                                    cp[3]};
    return true;
}

bool SetKeyValue(Doc::Document& document, std::string_view clip_id, int index, std::string field_id,
                 Doc::ParamValue value) {
    const Doc::Clip* clip = ClipById(document, clip_id);
    if (clip == nullptr) return false;
    const Doc::FieldDesc* desc =
        Doc::FindField(Doc::KeyFieldsFor(Doc::TypeOf(clip->command)), field_id);
    if (desc == nullptr || !desc->tweenable) return false;

    Doc::Key* key = MutableKey(document, clip_id, index);
    if (key == nullptr) return false;
    for (Doc::KeyValue& held : key->values) {
        if (held.id != field_id) continue;
        held.value = std::move(value);
        return true;
    }
    key->values.push_back(Doc::KeyValue{.id = std::move(field_id), .value = std::move(value)});
    return true;
}

bool UnsetKeyValue(Doc::Document& document, std::string_view clip_id, int index,
                   std::string_view field_id) {
    Doc::Key* key = MutableKey(document, clip_id, index);
    if (key == nullptr) return false;
    const std::size_t before = key->values.size();
    std::erase_if(key->values,
                  [field_id](const Doc::KeyValue& value) { return value.id == field_id; });
    return key->values.size() != before;
}

std::string NextDrawClip(const Doc::Document& document, std::string_view clip_id) {
    const Doc::Clip* clip = ClipById(document, clip_id);
    const Doc::Track* track = TrackOfClip(document, clip_id);
    if (clip == nullptr || track == nullptr) return {};
    if (Doc::TypeOf(clip->command) != Doc::CommandType::ModelDraw) return {};
    if (!clip->end.has_value()) return {};

    std::string best;
    int best_start = 0;
    for (const Doc::Track& other : document.tracks) {
        if (other.kind != Doc::TrackKind::Model || other.target != track->target) continue;
        for (const Doc::Clip& candidate : other.clips) {
            if (candidate.id == clip->id) continue;
            if (Doc::TypeOf(candidate.command) != Doc::CommandType::ModelDraw) continue;
            if (candidate.start < *clip->end) continue;
            if (!best.empty() && candidate.start >= best_start) continue;
            best = candidate.id;
            best_start = candidate.start;
        }
    }
    return best;
}

TransitionInsert AddTransition(Doc::Document& document, std::string_view a_id,
                               std::string_view b_id, int frames) {
    const ClipRef a_ref = FindClip(document, a_id);
    const Doc::Clip* a = ClipById(document, a_id);
    const Doc::Clip* b = ClipById(document, b_id);
    if (a == nullptr || b == nullptr || !a_ref.Valid() || !a->end.has_value()) return {};
    if (Doc::TypeOf(a->command) != Doc::CommandType::ModelDraw) return {};
    const auto* draw = std::get_if<Doc::ModelDraw>(&b->command);
    if (draw == nullptr || b->start < *a->end) return {};

    const int reach = std::max(1, frames);
    const int a_end = *a->end;
    const int b_start = b->start;
    const bool abutting = a_end == b_start;
    const int start = std::max(0, abutting ? a_end - reach : a_end);
    const int end = std::min(DocumentLength(document), b_start + reach);
    if (end <= start) return {};

    const Preset::Eval::ModelSlot* slot =
        SlotFor(StateAt(document, a_end - 1), document.tracks[(std::size_t)a_ref.track].target);
    if (slot == nullptr) return {};

    Doc::Clip tween;
    tween.id = UniqueClipId(document, std::string(a_id) + "_to_" + std::string(b_id));
    tween.start = start;
    tween.end = end;
    tween.label = "transition";
    tween.command = Doc::ModelTween{};
    tween.keys.push_back(PoseKey(*slot));
    tween.keys.push_back(ParamKey(*draw, end - start));

    TransitionInsert result;
    result.tween_id = tween.id;
    if (!abutting) {
        const std::string bridge = UniqueClipId(document, std::string(a_id) + "_hold");
        if (DuplicateClipTo(document, a_id, document.tracks[(std::size_t)a_ref.track].id, bridge,
                            a_end)) {
            Doc::Clip* held = MutableClip(document, bridge);
            held->end = b_start;
            held->label = "held for the transition";
            result.bridge_id = bridge;
        }
    }

    const int home = TweenTrackFor(document, a_ref.track, start, end);
    if (home >= 0) {
        document.tracks[(std::size_t)home].clips.push_back(std::move(tween));
        return result;
    }
    Doc::Track track;
    track.id = UniqueTrackId(document, document.tracks[(std::size_t)a_ref.track].target + "_tween");
    track.name = track.id;
    track.kind = Doc::TrackKind::Model;
    track.target = document.tracks[(std::size_t)a_ref.track].target;
    track.clips.push_back(std::move(tween));
    document.tracks.insert(document.tracks.begin() + a_ref.track + 1, std::move(track));
    return result;
}

}
