#include "preset/preset_convert.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/preset_asset_lengths.h"
#include "preset/preset_effective.h"
#include "preset/scene_preset.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Preset {

namespace {

using Doc::Clip;
using Doc::Key;
using Doc::KeyValue;
using Doc::Track;

struct PhaseInfo {
    std::string label;
    int start = 0;
    std::optional<int> end;
    Materialized mat;
    std::span<const Ramp> ramps;
    std::span<const Emitter> emitters;
};

struct TargetRef {
    enum class Scope : std::uint8_t {
        None,
        Model,
        Sprite,
        Camera,
    };

    Scope scope = Scope::None;
    std::string name;
    std::string field;
    int occurrence = 0;
};

Doc::Vec3 ToVec3(const std::array<float, 3>& value) {
    return {(double)value[0], (double)value[1], (double)value[2]};
}

std::string DirStem(std::string_view dir) {
    const std::size_t slash = dir.find_last_of("/\\");
    return std::string(slash == std::string_view::npos ? dir : dir.substr(slash + 1));
}

std::string Slug(std::string_view label) {
    std::string out;
    for (const char raw : label) {
        const char lower = (raw >= 'A' && raw <= 'Z') ? (char)(raw - 'A' + 'a') : raw;
        const bool word = (lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9');
        if (word) {
            out += lower;
        } else if (!out.empty() && out.back() != '_') {
            out += '_';
        }
    }
    while (!out.empty() && out.back() == '_')
        out.pop_back();
    return out;
}

TargetRef ParseTarget(std::string_view id) {
    TargetRef out;
    const std::size_t bracket = id.find('[');
    if (bracket == std::string_view::npos) {
        if (id.starts_with("camera.")) {
            out.scope = TargetRef::Scope::Camera;
            out.field = std::string(id.substr(7));
        }
        return out;
    }
    const std::size_t close = id.find(']', bracket);
    if (close == std::string_view::npos) return out;
    const std::string_view scope = id.substr(0, bracket);
    std::string_view name = id.substr(bracket + 1, close - bracket - 1);
    const std::size_t hash = name.find('#');
    if (hash != std::string_view::npos) {
        out.occurrence = 0;
        for (const char digit : name.substr(hash + 1))
            out.occurrence = (out.occurrence * 10) + (digit - '0');
        out.occurrence -= 1;
        name = name.substr(0, hash);
    }
    out.name = std::string(name);
    if (close + 1 < id.size() && id[close + 1] == '.')
        out.field = std::string(id.substr(close + 2));
    if (out.field.starts_with("motion.")) out.field = out.field.substr(7);
    if (scope == "model") out.scope = TargetRef::Scope::Model;
    if (scope == "sprite") out.scope = TargetRef::Scope::Sprite;
    return out;
}

bool SameSpriteParams(const Clip& a, const Clip& b) {
    return a.command == b.command && a.keys.empty() && b.keys.empty() && a.when == b.when;
}

class Converter {
public:
    Converter(const Scene& scene, const AssetLengths& lengths)
        : scene_(&scene), lengths_(&lengths) {}

    Doc::Document Build();

private:
    void BuildPhases();
    void BuildHeader();
    void BuildTargets();
    std::string AssetId(std::string_view dir, Doc::AssetKind kind);
    void BuildOptions();
    void BuildModelTracks();
    std::vector<Clip> GatedVariants(std::size_t model, std::size_t phase,
                                    const Doc::ModelDraw& base, const std::string& asset);
    void BuildModelDraws(std::size_t model, Track& draws);
    Clip SpriteClip(std::size_t sprite, std::size_t phase);
    [[nodiscard]] std::vector<std::pair<int, std::optional<int>>>
    VisibleSpans(std::size_t sprite) const;
    void BuildScrollClips(std::size_t sprite, Track& track);
    void BuildCameraSets();
    void BuildCameraTweens();
    void BuildModelTweens(std::size_t model);
    void BuildIntroTween();
    void BuildCountdownTween();
    void BuildModelMotion(std::size_t model);
    void BuildSpriteTracks();
    void BuildCameraTracks();
    void BuildFxTrack();
    void BuildSceneTrack();
    void BuildMarkers();
    [[nodiscard]] int NaturalLength() const;
    [[nodiscard]] int ClipFrames() const;
    std::string ClipId(std::string_view stem, std::size_t phase);
    [[nodiscard]] std::vector<const Ramp*> RampsFor(std::size_t phase, TargetRef::Scope scope,
                                                    std::string_view name, int occurrence) const;

    const Scene* scene_ = nullptr;
    const AssetLengths* lengths_ = nullptr;
    Doc::Document doc_;
    std::vector<PhaseInfo> phases_;
    std::vector<std::string> model_targets_;
    std::vector<std::string> sprite_targets_;
    std::vector<int> sprite_occurrence_;
    std::vector<std::string> clip_ids_;
};

std::string Converter::ClipId(std::string_view stem, std::size_t phase) {
    std::string base(stem);
    if (phase < phases_.size() && !phases_[phase].label.empty()) {
        const std::string slug = Slug(phases_[phase].label);
        if (!slug.empty()) {
            base += '_';
            base += slug;
        }
    }
    std::string candidate = base;
    int suffix = 2;
    while (std::ranges::find(clip_ids_, candidate) != clip_ids_.end()) {
        candidate = base + "_" + std::to_string(suffix);
        suffix++;
    }
    clip_ids_.push_back(candidate);
    return candidate;
}

void Converter::BuildPhases() {
    if (scene_->phases.empty()) {
        PhaseInfo single;
        single.mat = Materialize(*scene_, {}, {});
        phases_.push_back(std::move(single));
        return;
    }
    for (std::size_t i = 0; i < scene_->phases.size(); i++) {
        const Phase& phase = scene_->phases[i];
        PhaseInfo info;
        info.label = std::string(phase.label);
        info.start = phase.start_frame;
        if (i + 1 < scene_->phases.size()) info.end = scene_->phases[i + 1].start_frame;
        info.mat = Materialize(*scene_, phase.params, {});
        info.ramps = phase.ramps;
        info.emitters = phase.emitters;
        phases_.push_back(std::move(info));
    }
}

void Converter::BuildHeader() {
    const Effective& base = phases_.front().mat.effective;
    doc_.id = std::string(scene_->id);
    doc_.name = std::string(scene_->name);
    doc_.build = std::string(scene_->build);
    doc_.fps = 60;
    doc_.render = Doc::RenderSpec{.width = scene_->render_w,
                                  .height = scene_->render_h,
                                  .opaque = scene_->opaque_screen,
                                  .shading = (Doc::Shading)(int)scene_->shading,
                                  .sprite_split_priority = scene_->sprite_split_priority};
    doc_.camera =
        Doc::CameraSpec{.eye = ToVec3(scene_->camera.eye),
                        .at = ToVec3(scene_->camera.at),
                        .up = ToVec3(scene_->camera.up),
                        .fov_y = (double)scene_->camera.fov_y,
                        .near_z = (double)scene_->camera.near_z,
                        .far_z = (double)scene_->camera.far_z,
                        .aspect = Doc::AspectSpec{.automatic = scene_->camera.aspect == 0.0F,
                                                  .value = (double)scene_->camera.aspect}};
    for (const DirectionalLight& light : base.lights) {
        doc_.lights.push_back(Doc::LightSpec{.direction = ToVec3(light.direction),
                                             .diffuse = ToVec3(light.diffuse),
                                             .specular = ToVec3(light.specular)});
    }
    doc_.rng_seed = scene_->rng_seed;
}

std::string Converter::AssetId(std::string_view dir, Doc::AssetKind kind) {
    for (const Doc::Asset& asset : doc_.assets) {
        if (asset.dir == dir) return asset.id;
    }
    const std::string stem = DirStem(dir);
    std::string candidate = stem;
    int suffix = 2;
    while (std::ranges::any_of(doc_.assets,
                               [&](const Doc::Asset& asset) { return asset.id == candidate; })) {
        candidate = stem + "_" + std::to_string(suffix);
        suffix++;
    }
    doc_.assets.push_back(Doc::Asset{.id = candidate, .kind = kind, .dir = std::string(dir)});
    return candidate;
}

void Converter::BuildTargets() {
    const Effective& base = phases_.front().mat.effective;
    for (const ModelState& model : base.models) {
        model_targets_.push_back(model.model);
        AssetId(model.scene_dir, Doc::AssetKind::Scene3d);
    }
    for (std::size_t i = 0; i < base.sprites.size(); i++) {
        const SpriteState& sprite = base.sprites[i];
        int seen = 0;
        for (std::size_t j = 0; j < i; j++) {
            if (base.sprites[j].sprite == sprite.sprite) seen++;
        }
        sprite_occurrence_.push_back(seen);
        sprite_targets_.push_back(seen == 0 ? sprite.sprite
                                            : sprite.sprite + "_" + std::to_string(seen + 1));
        AssetId(sprite.package_dir, Doc::AssetKind::Package2d);
    }
    for (const PhaseInfo& phase : phases_) {
        for (const Emitter& emitter : phase.emitters)
            AssetId(emitter.package_dir, Doc::AssetKind::Package2d);
    }
}

void Converter::BuildOptions() {
    if (scene_->options.empty()) return;
    const Option& option = scene_->options.front();
    Doc::OptionSpec spec;
    spec.id = std::string(option.id);
    spec.label = std::string(option.label);
    spec.default_choice = option.default_choice;
    spec.transition = Doc::Transition{.frames = option.transition_frames,
                                      .step = 4,
                                      .ease = Doc::Ease::Linear,
                                      .spin_kick = (double)option.spin_kick};
    const std::string lead = model_targets_.empty() ? std::string() : model_targets_.front();
    for (const OptionChoice& choice : option.choices) {
        Doc::ChoiceSpec entry;
        entry.label = std::string(choice.label);
        if (!lead.empty()) {
            entry.values.push_back(Doc::ChoiceValue{.id = "model[" + lead + "].position",
                                                    .value = ToVec3(choice.position)});
        }
        if (choice.moves_camera) {
            entry.values.push_back(
                Doc::ChoiceValue{.id = "camera.eye", .value = ToVec3(choice.camera_eye)});
        }
        spec.choices.push_back(std::move(entry));
    }
    doc_.options.push_back(std::move(spec));
}

std::vector<const Ramp*> Converter::RampsFor(std::size_t phase, TargetRef::Scope scope,
                                             std::string_view name, int occurrence) const {
    std::vector<const Ramp*> out;
    for (const Ramp& ramp : phases_[phase].ramps) {
        const TargetRef ref = ParseTarget(ramp.id);
        if (ref.scope != scope) continue;
        if (scope != TargetRef::Scope::Camera &&
            (ref.name != name || ref.occurrence != occurrence)) {
            continue;
        }
        out.push_back(&ramp);
    }
    return out;
}

std::vector<Key> RampKeys(const Ramp& ramp, std::string_view field) {
    const bool sine = ramp.curve == Curve::Sine;
    Key first{.at = 0, .ease = sine ? Doc::Ease::SineDeg : Doc::Ease::Linear};
    if (sine) first.rate_deg = (double)ramp.degrees_per_frame;
    Key last{.at = std::max(1, ramp.frames)};
    const bool scalar = field == "alpha" || field == "scale";
    if (scalar) {
        first.values.push_back(KeyValue{.id = std::string(field), .value = (double)ramp.from[0]});
        last.values.push_back(KeyValue{.id = std::string(field), .value = (double)ramp.to[0]});
    } else {
        first.values.push_back(KeyValue{.id = std::string(field), .value = ToVec3(ramp.from)});
        last.values.push_back(KeyValue{.id = std::string(field), .value = ToVec3(ramp.to)});
    }
    return {std::move(first), std::move(last)};
}

int Converter::ClipFrames() const {
    const Effective& base = phases_.front().mat.effective;
    if (base.models.empty()) return 0;
    const float speed = base.models.front().anim_speed;
    const float ticks = lengths_->MaxTime(base.models.front().scene_dir);
    if (speed <= 0.0F || ticks <= 0.0F) return 0;
    return (int)std::lround(ticks / speed);
}

int Converter::NaturalLength() const {
    if (scene_->countdown.start_frames > 0) return scene_->countdown.start_frames;
    if (scene_->phases.empty()) return ClipFrames();
    const PhaseInfo& last = phases_.back();
    int tail = ClipFrames();
    for (const Ramp& ramp : last.ramps)
        tail = std::max(tail, ramp.frames);
    for (const SpriteState& sprite : last.mat.effective.sprites) {
        if (!sprite.visible || !sprite.animated) continue;
        tail = std::max(tail, lengths_->AnimationLength(sprite.package_dir, sprite.sprite));
    }
    return last.start + tail;
}

void Converter::BuildMarkers() {
    for (const PhaseInfo& phase : phases_) {
        if (phase.label.empty()) continue;
        doc_.markers.push_back(Doc::Marker{.frame = phase.start, .label = phase.label});
    }
}

Doc::ModelDraw DrawFrom(const ModelState& model, const std::string& asset) {
    return Doc::ModelDraw{.asset = asset,
                          .blend_mode = (Doc::ModelBlend)model.blend_mode,
                          .alpha = (double)model.alpha,
                          .anim_speed = (double)model.anim_speed,
                          .position = ToVec3(model.position),
                          .rotation = ToVec3(model.rotation),
                          .scale = ToVec3(model.scale),
                          .spin_per_frame = ToVec3(model.motion.spin_per_frame)};
}

std::vector<ParamOverride> WithChoice(std::span<const ParamOverride> phase,
                                      std::span<const ParamOverride> choice) {
    std::vector<ParamOverride> out(phase.begin(), phase.end());
    out.insert(out.end(), choice.begin(), choice.end());
    return out;
}

std::vector<Clip> Converter::GatedVariants(std::size_t model, std::size_t phase,
                                           const Doc::ModelDraw& base, const std::string& asset) {
    std::vector<Clip> gated;
    if (scene_->options.empty()) return gated;
    for (const OptionChoice& choice : scene_->options.front().choices) {
        if (choice.params.empty()) continue;
        const std::vector<ParamOverride> merged =
            WithChoice(scene_->phases.empty() ? std::span<const ParamOverride>()
                                              : scene_->phases[phase].params,
                       choice.params);
        const Materialized mat = Materialize(*scene_, merged, {});
        const Doc::ModelDraw variant = DrawFrom(mat.effective.models[model], asset);
        if (variant == base) continue;
        Clip clip;
        clip.id = ClipId(model_targets_[model] + "_" + Slug(choice.label), phase);
        clip.start = phases_[phase].start;
        clip.end = phases_[phase].end;
        clip.when = Doc::Gate{.option = doc_.options.front().id,
                              .kind = Doc::GateKind::Choice,
                              .choices = {std::string(choice.label)}};
        clip.command = variant;
        gated.push_back(std::move(clip));
    }
    return gated;
}

void Converter::BuildModelDraws(std::size_t model, Track& draws) {
    const std::string& target = model_targets_[model];
    const std::string asset =
        AssetId(phases_.front().mat.effective.models[model].scene_dir, Doc::AssetKind::Scene3d);
    for (std::size_t p = 0; p < phases_.size(); p++) {
        const ModelState& state = phases_[p].mat.effective.models[model];
        if (!state.visible) continue;
        const Doc::ModelDraw base = DrawFrom(state, asset);
        std::vector<Clip> gated = GatedVariants(model, p, base, asset);
        Clip clip;
        clip.id = ClipId(target, p);
        clip.start = phases_[p].start;
        clip.end = phases_[p].end;
        clip.command = base;
        if (!gated.empty()) {
            std::vector<std::string> labels;
            labels.reserve(gated.size());
            for (const Clip& variant : gated)
                labels.push_back(variant.when->choices.front());
            clip.when = Doc::Gate{.option = doc_.options.front().id,
                                  .kind = Doc::GateKind::Not,
                                  .choices = std::move(labels)};
        }
        draws.clips.push_back(std::move(clip));
        for (Clip& variant : gated)
            draws.clips.push_back(std::move(variant));
    }
}

void Converter::BuildModelTracks() {
    for (std::size_t m = 0; m < model_targets_.size(); m++) {
        const std::string& target = model_targets_[m];
        Track draws{.id = target, .name = target, .kind = Doc::TrackKind::Model, .target = target};
        BuildModelDraws(m, draws);
        doc_.tracks.push_back(std::move(draws));
        BuildModelTweens(m);
        BuildModelMotion(m);
    }
}

void Converter::BuildModelTweens(std::size_t model) {
    const std::string& target = model_targets_[model];
    std::vector<Track> tracks;
    for (std::size_t p = 0; p < phases_.size(); p++) {
        const std::vector<const Ramp*> ramps = RampsFor(p, TargetRef::Scope::Model, target, 0);
        std::vector<Clip> clips;
        for (const Ramp* ramp : ramps) {
            const TargetRef ref = ParseTarget(ramp->id);
            const std::vector<Key> keys = RampKeys(*ramp, ref.field);
            Clip* host = nullptr;
            for (Clip& clip : clips) {
                if (clip.keys.front().ease == keys.front().ease &&
                    clip.keys.front().rate_deg == keys.front().rate_deg &&
                    clip.keys.back().at == keys.back().at) {
                    host = &clip;
                }
            }
            if (host == nullptr) {
                Clip clip;
                clip.start = phases_[p].start;
                clip.end = phases_[p].end;
                clip.command = Doc::ModelTween{};
                clip.keys = keys;
                clips.push_back(std::move(clip));
                continue;
            }
            host->keys.front().values.push_back(keys.front().values.front());
            host->keys.back().values.push_back(keys.back().values.front());
        }
        for (std::size_t i = 0; i < clips.size(); i++) {
            while (tracks.size() <= i) {
                const std::string id =
                    target + "_tween" +
                    (tracks.empty() ? "" : "_" + std::to_string(tracks.size() + 1));
                tracks.push_back(
                    Track{.id = id, .name = id, .kind = Doc::TrackKind::Model, .target = target});
            }
            clips[i].id = ClipId(target + "_tween", p);
            tracks[i].clips.push_back(std::move(clips[i]));
        }
    }
    for (Track& track : tracks)
        doc_.tracks.push_back(std::move(track));
    if (model != 0) return;
    BuildIntroTween();
    BuildCountdownTween();
}

void Converter::BuildIntroTween() {
    const std::string& target = model_targets_.front();
    Track track{.id = target + "_intro",
                .name = target + "_intro",
                .kind = Doc::TrackKind::Model,
                .target = target};
    for (std::size_t p = 0; p < phases_.size(); p++) {
        const Intro& intro = phases_[p].mat.effective.intro;
        if (intro.frames <= 0 || intro.speed_to == intro.speed_from) continue;
        Clip clip;
        clip.id = ClipId(target + "_intro", p);
        clip.start = phases_[p].start;
        clip.end = phases_[p].end;
        clip.command = Doc::ModelTween{};
        clip.keys.push_back(
            Key{.at = 0,
                .ease = Doc::Ease::Linear,
                .values = {KeyValue{.id = "anim_speed", .value = (double)intro.speed_from}}});
        clip.keys.push_back(
            Key{.at = intro.frames,
                .values = {KeyValue{.id = "anim_speed", .value = (double)intro.speed_to}}});
        track.clips.push_back(std::move(clip));
    }
    if (!track.clips.empty()) doc_.tracks.push_back(std::move(track));
}

void Converter::BuildCountdownTween() {
    const Countdown& countdown = scene_->countdown;
    if (countdown.start_frames <= 0 || countdown.ramp_below <= 1) return;
    const std::string& target = model_targets_.front();
    const auto base = (double)countdown.speed_base;
    const auto per_frame = (double)countdown.speed_per_frame;
    const auto fade_from = (double)countdown.fade_from;
    const auto fade_per_frame = (double)countdown.fade_per_frame;
    const int last = countdown.ramp_below - 1;
    Key first{.at = 0, .ease = Doc::Ease::Linear};
    first.values.push_back(KeyValue{.id = "anim_speed", .value = base + per_frame});
    first.values.push_back(KeyValue{.id = "blend_mode", .value = (int)countdown.ramp_blend_mode});
    Key end{.at = countdown.ramp_below - 2};
    end.values.push_back(KeyValue{.id = "anim_speed", .value = base + ((double)last * per_frame)});
    if (fade_per_frame > 0.0) {
        first.values.push_back(
            KeyValue{.id = "alpha", .value = std::clamp(fade_from - fade_per_frame, 0.0, 1.0)});
        end.values.push_back(
            KeyValue{.id = "alpha",
                     .value = std::clamp(fade_from - ((double)last * fade_per_frame), 0.0, 1.0)});
    }
    Clip clip;
    clip.id = target + "_countdown";
    clip_ids_.push_back(clip.id);
    clip.start = countdown.start_frames - countdown.ramp_below + 1;
    clip.command = Doc::ModelTween{};
    clip.keys.push_back(std::move(first));
    clip.keys.push_back(std::move(end));
    doc_.tracks.push_back(Track{.id = target + "_countdown",
                                .name = target + "_countdown",
                                .kind = Doc::TrackKind::Model,
                                .target = target,
                                .clips = {std::move(clip)}});
}

void Converter::BuildModelMotion(std::size_t model) {
    const std::string& target = model_targets_[model];
    const ModelMotion& motion = phases_.front().mat.effective.models[model].motion;
    if (motion.orbit_radius > 0.0F || motion.spin_kick != 0.0F || motion.spin_kick_decay != 0.0F) {
        Doc::ModelMotionCmd command;
        if (motion.orbit_radius > 0.0F) {
            command.orbit = Doc::Orbit{.radius = (double)motion.orbit_radius,
                                       .rate_rad_per_frame = (double)motion.orbit_rate,
                                       .center = {(double)motion.center_x, (double)motion.center_y},
                                       .z_start = (double)motion.z_start,
                                       .z_per_frame = (double)motion.z_per_frame,
                                       .z_min = (double)motion.z_min};
        }
        command.spin_kick = (double)motion.spin_kick;
        command.spin_kick_decay = (double)motion.spin_kick_decay;
        Clip clip;
        clip.id = target + "_motion";
        clip_ids_.push_back(clip.id);
        clip.start = 0;
        clip.command = command;
        doc_.tracks.push_back(Track{.id = target + "_motion",
                                    .name = target + "_motion",
                                    .kind = Doc::TrackKind::Model,
                                    .target = target,
                                    .clips = {std::move(clip)}});
    }
    Track pulse{.id = target + "_pulse",
                .name = target + "_pulse",
                .kind = Doc::TrackKind::Model,
                .target = target};
    for (std::size_t p = 0; p < phases_.size(); p++) {
        const Pulse& spec = phases_[p].mat.effective.pulse;
        if (spec.scale_odd == 1.0F && spec.scale_even == 1.0F) continue;
        Doc::ModelMotionCmd command;
        command.pulse = Doc::PulseSpec{.grid = (Doc::Grid)(int)spec.grid,
                                       .scale_odd = (double)spec.scale_odd,
                                       .scale_even = (double)spec.scale_even,
                                       .frames = spec.frames};
        Clip clip;
        clip.id = ClipId(target + "_pulse", p);
        clip.start = phases_[p].start;
        clip.end = phases_[p].end;
        clip.command = command;
        pulse.clips.push_back(std::move(clip));
    }
    if (!pulse.clips.empty()) doc_.tracks.push_back(std::move(pulse));
}

std::vector<std::pair<int, std::optional<int>>> Converter::VisibleSpans(std::size_t sprite) const {
    std::vector<std::pair<int, std::optional<int>>> spans;
    for (const PhaseInfo& phase : phases_) {
        if (!phase.mat.effective.sprites[sprite].visible) continue;
        if (!spans.empty() && spans.back().second.has_value() &&
            *spans.back().second == phase.start) {
            spans.back().second = phase.end;
            continue;
        }
        spans.emplace_back(phase.start, phase.end);
    }
    return spans;
}

Clip Converter::SpriteClip(std::size_t sprite, std::size_t phase) {
    const SpriteState& state = phases_[phase].mat.effective.sprites[sprite];
    const std::string asset = AssetId(state.package_dir, Doc::AssetKind::Package2d);
    Clip clip;
    clip.start = phases_[phase].start;
    clip.end = phases_[phase].end;
    if (state.animated) {
        clip.command = Doc::SpriteAnimate{.asset = asset,
                                          .animation = state.sprite,
                                          .x = (double)state.x,
                                          .y = (double)state.y,
                                          .alpha = (double)state.alpha,
                                          .scale = (double)state.scale,
                                          .priority = state.priority,
                                          .playback = state.timing.playback,
                                          .loop_start = state.timing.loop_start,
                                          .loop_end = state.timing.loop_end,
                                          .hidden_parts = state.hidden_parts};
    } else {
        clip.command = Doc::SpriteDraw{.asset = asset,
                                       .cell = state.sprite,
                                       .x = (double)state.x,
                                       .y = (double)state.y,
                                       .alpha = (double)state.alpha,
                                       .scale = (double)state.scale,
                                       .blend = (Doc::SpriteBlend)state.blend,
                                       .priority = state.priority};
    }
    for (const Ramp* ramp :
         RampsFor(phase, TargetRef::Scope::Sprite, state.sprite, sprite_occurrence_[sprite])) {
        const TargetRef ref = ParseTarget(ramp->id);
        for (Key& key : RampKeys(*ramp, ref.field))
            clip.keys.push_back(std::move(key));
    }
    return clip;
}

void Converter::BuildScrollClips(std::size_t sprite, Track& track) {
    const SpriteState& first = phases_.front().mat.effective.sprites[sprite];
    if (first.scroll_x == 0.0F && first.scroll_wrap == 0.0F) return;
    const std::vector<std::pair<int, std::optional<int>>> spans = VisibleSpans(sprite);
    for (const auto& [start, end] : spans) {
        Clip clip;
        clip.id = sprite_targets_[sprite] + "_scroll" +
                  (spans.size() > 1 ? std::to_string(start) : std::string());
        clip_ids_.push_back(clip.id);
        clip.start = start;
        clip.end = end;
        clip.command = Doc::SpriteScroll{.scroll_x = (double)first.scroll_x,
                                         .scroll_wrap = (double)first.scroll_wrap};
        track.clips.push_back(std::move(clip));
    }
}

void Converter::BuildSpriteTracks() {
    for (std::size_t s = 0; s < sprite_targets_.size(); s++) {
        const std::string& target = sprite_targets_[s];
        Track track{.id = target, .name = target, .kind = Doc::TrackKind::Sprite, .target = target};
        for (std::size_t p = 0; p < phases_.size(); p++) {
            if (!phases_[p].mat.effective.sprites[s].visible) continue;
            Clip clip = SpriteClip(s, p);
            if (!track.clips.empty() && track.clips.back().end.has_value() &&
                *track.clips.back().end == clip.start &&
                SameSpriteParams(track.clips.back(), clip)) {
                track.clips.back().end = clip.end;
                continue;
            }
            clip.id = ClipId(target, p);
            track.clips.push_back(std::move(clip));
        }
        BuildScrollClips(s, track);
        if (!track.clips.empty()) doc_.tracks.push_back(std::move(track));
    }
}

bool CameraDifference(const Effective& effective, const Camera& base, Doc::CameraSet& out) {
    bool differs = false;
    if (effective.camera.eye != base.eye) {
        out.eye = ToVec3(effective.camera.eye);
        differs = true;
    }
    if (effective.camera.at != base.at) {
        out.at = ToVec3(effective.camera.at);
        differs = true;
    }
    if (effective.camera.up != base.up) {
        out.up = ToVec3(effective.camera.up);
        differs = true;
    }
    if (effective.camera.fov_y != base.fov_y) {
        out.fov_y = (double)effective.camera.fov_y;
        differs = true;
    }
    if (effective.camera.near_z != base.near_z) {
        out.near_z = (double)effective.camera.near_z;
        differs = true;
    }
    if (effective.camera.far_z != base.far_z) {
        out.far_z = (double)effective.camera.far_z;
        differs = true;
    }
    const bool automatic = base.aspect == 0.0F;
    if (effective.aspect_auto != automatic ||
        (!effective.aspect_auto && effective.aspect_value != base.aspect)) {
        out.aspect = Doc::AspectSpec{.automatic = effective.aspect_auto,
                                     .value = (double)effective.aspect_value};
        differs = true;
    }
    return differs;
}

void Converter::BuildCameraSets() {
    Track sets{.id = "camera", .name = "camera", .kind = Doc::TrackKind::Camera};
    for (std::size_t p = 0; p < phases_.size(); p++) {
        Doc::CameraSet command;
        if (!CameraDifference(phases_[p].mat.effective, scene_->camera, command)) continue;
        Clip clip;
        clip.start = phases_[p].start;
        clip.end = phases_[p].end;
        clip.command = command;
        if (!sets.clips.empty() && sets.clips.back().end.has_value() &&
            *sets.clips.back().end == clip.start && sets.clips.back().command == clip.command) {
            sets.clips.back().end = clip.end;
            continue;
        }
        clip.id = ClipId("camera", p);
        sets.clips.push_back(std::move(clip));
    }
    if (!sets.clips.empty()) doc_.tracks.push_back(std::move(sets));
}

void Converter::BuildCameraTweens() {
    Track tweens{.id = "camera_tween", .name = "camera_tween", .kind = Doc::TrackKind::Camera};
    for (std::size_t p = 0; p < phases_.size(); p++) {
        Clip clip;
        clip.start = phases_[p].start;
        clip.end = phases_[p].end;
        clip.command = Doc::CameraTween{};
        for (const Ramp* ramp : RampsFor(p, TargetRef::Scope::Camera, {}, 0)) {
            const TargetRef ref = ParseTarget(ramp->id);
            for (Key& key : RampKeys(*ramp, ref.field))
                clip.keys.push_back(std::move(key));
        }
        const Intro& intro = phases_[p].mat.effective.intro;
        if (intro.frames > 0 && intro.fov_from > 0.0F) {
            clip.keys.push_back(
                Key{.at = 0,
                    .ease = Doc::Ease::Linear,
                    .values = {KeyValue{.id = "fov_y", .value = (double)intro.fov_from}}});
            clip.keys.push_back(
                Key{.at = intro.frames,
                    .values = {KeyValue{.id = "fov_y", .value = (double)intro.fov_to}}});
        }
        if (clip.keys.empty()) continue;
        clip.id = ClipId("camera_tween", p);
        tweens.clips.push_back(std::move(clip));
    }
    if (!tweens.clips.empty()) doc_.tracks.push_back(std::move(tweens));
}

void Converter::BuildCameraTracks() {
    BuildCameraSets();
    BuildCameraTweens();
}

void Converter::BuildFxTrack() {
    Track track{.id = "fx", .name = "fx", .kind = Doc::TrackKind::Fx};
    for (std::size_t p = 0; p < phases_.size(); p++) {
        for (const Emitter& emitter : phases_[p].emitters) {
            Doc::EmitterCmd command;
            command.asset = AssetId(emitter.package_dir, Doc::AssetKind::Package2d);
            command.cell = std::string(emitter.cell);
            command.spawn = (Doc::Spawn)(int)emitter.spawn;
            command.count = emitter.count;
            command.angle_step_deg = (double)emitter.angle_step_deg;
            command.phase_rate_deg = (double)emitter.phase_rate_deg;
            command.phase_amplitude_deg = (double)emitter.phase_amplitude_deg;
            command.radius_from = emitter.radius_from;
            command.radius_to = emitter.radius_to;
            command.reach_frames = emitter.frames;
            const auto center_x = (double)emitter.center_x;
            const auto center_y = (double)emitter.center_y;
            if (center_x != (double)scene_->render_w * 0.5 ||
                center_y != (double)scene_->render_h * 0.5) {
                command.center = Doc::Vec2{center_x, center_y};
            }
            command.priority = emitter.priority;
            command.blend = (Doc::SpriteBlend)emitter.blend;
            command.scale_percent = emitter.scale;
            if (emitter.scatter) {
                command.scatter =
                    Doc::Scatter{.span = {(double)emitter.span_x, (double)emitter.span_y},
                                 .offset = {(double)emitter.offset_x, (double)emitter.offset_y}};
            }
            command.beat_grid = (Doc::Grid)(int)emitter.beat_grid;
            command.beat_odd = emitter.beat_odd;
            command.life = emitter.life;
            command.life_base = emitter.life_base;
            command.life_span = emitter.life_span;
            Clip clip;
            clip.id = ClipId("fx", p);
            clip.start = phases_[p].start;
            clip.end = phases_[p].end;
            clip.command = std::move(command);
            track.clips.push_back(std::move(clip));
        }
    }
    if (!track.clips.empty()) doc_.tracks.push_back(std::move(track));
}

void Converter::BuildSceneTrack() {
    Track track{.id = "scene", .name = "scene", .kind = Doc::TrackKind::Scene};
    if (scene_->beat.rate > 0) {
        Clip clip;
        clip.id = "beat";
        clip_ids_.push_back(clip.id);
        clip.start = 0;
        clip.command = Doc::RhythmBeat{.rate = scene_->beat.rate,
                                       .span = scene_->beat.span,
                                       .offset_a = scene_->beat.offset_a,
                                       .offset_b = scene_->beat.offset_b};
        track.clips.push_back(std::move(clip));
    }
    for (std::size_t p = 0; p < phases_.size(); p++) {
        const Jitter& jitter = phases_[p].mat.effective.jitter;
        if (jitter.span <= 0) continue;
        const int start = std::max(phases_[p].start, jitter.from_frame + 1);
        if (phases_[p].end.has_value() && start >= *phases_[p].end) continue;
        Clip clip;
        clip.id = ClipId("jitter", p);
        clip.start = start;
        clip.end = phases_[p].end;
        clip.command = Doc::RhythmJitter{
            .span = jitter.span, .scale = (double)jitter.scale, .mode = Doc::JitterMode::Set};
        track.clips.push_back(std::move(clip));
    }
    if (!track.clips.empty()) doc_.tracks.push_back(std::move(track));
}

Doc::Document Converter::Build() {
    BuildPhases();
    BuildHeader();
    BuildTargets();
    BuildOptions();
    BuildSpriteTracks();
    BuildModelTracks();
    BuildCameraTracks();
    BuildFxTrack();
    BuildSceneTrack();
    BuildMarkers();
    doc_.length = NaturalLength();
    return std::move(doc_);
}

}

Doc::Document FromScene(const Scene& scene, const AssetLengths& lengths) {
    Converter converter(scene, lengths);
    return converter.Build();
}

}
