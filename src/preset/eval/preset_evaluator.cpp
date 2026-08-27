#include "preset/eval/preset_evaluator.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/eval_camera_lights.h"
#include "preset/eval/eval_ease.h"
#include "preset/eval/eval_emit.h"
#include "preset/eval/eval_models.h"
#include "preset/eval/eval_particles.h"
#include "preset/eval/eval_push.h"
#include "preset/eval/eval_resolve.h"
#include "preset/eval/eval_scene.h"
#include "preset/eval/eval_state.h"
#include "preset/eval/eval_tween.h"
#include "preset/eval/frame_state.h"
#include "preset/preset_asset_lengths.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Preset::Eval {

namespace {

constexpr int kCheckpointStride = 256;

bool MovesCamera(const Doc::Document& document, const std::vector<int>& choices) {
    for (std::size_t i = 0; i < document.options.size(); i++) {
        const Doc::OptionSpec& option = document.options[i];
        const int index = SelectedChoice(option, choices, i);
        if (index < 0) continue;
        for (const Doc::ChoiceValue& value : option.choices[(std::size_t)index].values) {
            if (value.id == "camera.eye") return true;
        }
    }
    return false;
}

std::vector<float> Ticks(const std::vector<ModelRuntime>& models) {
    std::vector<float> out;
    out.reserve(models.size());
    for (const ModelRuntime& runtime : models)
        out.push_back(runtime.tick);
    return out;
}

Push DrawRange(int low, int high) {
    Push push;
    push.call = PushCall::DrawSprites;
    push.index = low;
    push.index2 = high;
    return push;
}

Push TimedPush(PushCall call, float dt) {
    Push push;
    push.call = call;
    push.value = dt;
    return push;
}

}

void Evaluator::Load(std::shared_ptr<const Doc::Document> document, Preset::AssetLengths lengths) {
    document_ = std::move(document);
    lengths_ = std::move(lengths);
    boundaries_.clear();
    if (document_ != nullptr) {
        for (const Doc::Track& track : document_->tracks) {
            for (const Doc::Clip& clip : track.clips) {
                const Doc::CommandType type = Doc::TypeOf(clip.command);
                if (Doc::IsEvent(type) || Doc::TraitsFor(type).family == Doc::Family::None)
                    continue;
                if (clip.start > 0) boundaries_.insert(clip.start);
                if (clip.end.has_value() && *clip.end > 0) boundaries_.insert(*clip.end);
            }
        }
    }
    Reset();
}

std::string Evaluator::AssetDir(const std::string& asset_id) const {
    if (document_ == nullptr) return {};
    for (const Doc::Asset& asset : document_->assets) {
        if (asset.id == asset_id) return asset.dir;
    }
    return {};
}

FrameState Evaluator::Resolve(int frame) const {
    return ResolveWithout(frame, true);
}

FrameState Evaluator::ResolveWithout(int frame, bool tweens) const {
    const ResolveInput input{
        .document = document_.get(), .choices = &state_.choices, .tweens = tweens};
    return ResolveFrame(input, frame);
}

void Evaluator::ApplyChoiceValues(FrameState& state) const {
    if (document_ == nullptr) return;
    const Doc::Document& document = *document_;
    for (std::size_t i = 0; i < document.options.size(); i++) {
        const Doc::OptionSpec& option = document.options[i];
        const int index = SelectedChoice(option, state_.choices, i);
        if (index < 0) continue;
        const bool blending = std::cmp_equal(i, state_.transition_option) &&
                              option.transition.frames > 0 && state_.transition > 0;
        const float factor =
            blending ? 1.0F - ((float)state_.transition / (float)option.transition.frames) : 1.0F;
        for (const Doc::ChoiceValue& value : option.choices[(std::size_t)index].values) {
            const TweenValue target = OverrideToTween(value.value);
            TweenValue from = target;
            if (blending) {
                for (const CapturedValue& captured : state_.captured) {
                    if (captured.id == value.id) from = captured.value;
                }
            }
            WriteTarget(value.id, blending ? BlendValues(from, target, factor) : target, state,
                        nullptr);
        }
    }
}

void Evaluator::ResetRuntime() {
    state_ = EvalState{};
    if (document_ == nullptr) return;
    state_.rng.Seed(document_->rng_seed);
    for (const Doc::OptionSpec& option : document_->options)
        state_.choices.push_back(option.default_choice);
    current_ = Resolve(0);
    ApplyChoiceValues(current_);
    state_.models.assign(current_.models.size(), ModelRuntime{});
    state_.sprite_clock.assign(current_.sprites.size(), 0.0F);
    state_.sprite_start.assign(current_.sprites.size(), -1);
    model_max_time_.assign(current_.models.size(), 0.0F);
    for (std::size_t i = 0; i < current_.models.size(); i++) {
        const ModelSlot& slot = current_.models[i];
        state_.models[i].draw_start = slot.draw_start;
        state_.models[i].motion_start = slot.motion_start;
        state_.models[i].kick = std::max(1.0F, slot.spin_kick);
        model_max_time_[i] = lengths_.MaxTime(AssetDir(slot.asset));
    }
    for (std::size_t i = 0; i < current_.sprites.size(); i++) {
        state_.sprite_start[i] = current_.sprites[i].draw_start;
        state_.sprite_clock[i] = (float)current_.sprites[i].offset;
    }
    if (FireSelects(current_)) {
        current_ = Resolve(state_.frame);
        ApplyChoiceValues(current_);
    }
    StepEases(current_, true);
}

bool Evaluator::FireSelects(const FrameState& state) {
    if (document_ == nullptr) return false;
    bool fired = false;
    for (const Doc::Clip* clip : state.selects) {
        const auto& select = std::get<Doc::OptionSelect>(clip->command);
        for (std::size_t i = 0; i < document_->options.size(); i++) {
            const Doc::OptionSpec& option = document_->options[i];
            if (option.id != select.option) continue;
            for (std::size_t choice = 0; choice < option.choices.size(); choice++) {
                if (option.choices[choice].label != select.choice) continue;
                if (SelectedChoice(option, state_.choices, i) == (int)choice) continue;
                SetOption((int)i, (int)choice);
                fired = true;
            }
        }
    }
    return fired;
}

int Evaluator::TransitionStep() const {
    if (document_ == nullptr || document_->options.empty()) return 1;
    const auto index =
        (std::size_t)std::clamp(state_.transition_option, 0, (int)document_->options.size() - 1);
    return std::max(1, document_->options[index].transition.step);
}

void Evaluator::Reset() {
    ResetRuntime();
    checkpoints_.clear();
    checkpoints_[0] = state_;
}

int Evaluator::DerivedLength() const {
    int ends = 0;
    int tails = 0;
    for (const Doc::Track& track : document_->tracks) {
        for (const Doc::Clip& clip : track.clips) {
            if (clip.end.has_value()) ends = std::max(ends, *clip.end);
            if (const auto* animate = std::get_if<Doc::SpriteAnimate>(&clip.command)) {
                tails =
                    std::max(tails, clip.start + lengths_.AnimationLength(AssetDir(animate->asset),
                                                                          animate->animation));
            }
            const auto* draw = std::get_if<Doc::ModelDraw>(&clip.command);
            if (draw == nullptr || draw->anim_speed <= 0.0) continue;
            const float ticks = lengths_.MaxTime(AssetDir(draw->asset));
            tails = std::max(tails, clip.start + (int)std::lround(ticks / draw->anim_speed));
        }
    }
    return (ends > 0) ? ends : tails;
}

int Evaluator::Length() const {
    if (document_ == nullptr) return 0;
    if (document_->length.has_value()) return *document_->length;
    return DerivedLength();
}

std::vector<Push> Evaluator::Seek(int frame) {
    const int last = std::max(0, Length() - 1);
    const int target = std::clamp(frame, 0, last);
    int start = 0;
    for (const auto& [at, snapshot] : checkpoints_) {
        if (at <= target && at >= start) start = at;
    }
    if (target < state_.frame || start > state_.frame) {
        const auto found = checkpoints_.find(start);
        if (found == checkpoints_.end()) {
            ResetRuntime();
        } else {
            state_ = found->second;
            current_ = Resolve(state_.frame);
            ApplyChoiceValues(current_);
            StepEases(current_, false);
        }
    }
    while (state_.frame < target)
        AdvanceFrame();
    return Rebind();
}

std::vector<Push> Evaluator::Rebind() const {
    std::vector<Push> out;
    if (document_ == nullptr) return out;
    EmitRebind(current_, out);
    for (std::size_t i = 0; i < current_.models.size() && i < state_.models.size(); i++) {
        const ModelSlot& slot = current_.models[i];
        if (!slot.visible) continue;
        const Vec3f position = PlacedPosition(slot, current_.jitter, state_.jitter, current_.frame);
        Push push = VectorPush(PushCall::SetModelTransform, slot.name, position, true);
        for (std::size_t axis = 0; axis < push.vec_b.size(); axis++) {
            push.vec_b[axis] = slot.rotation[axis] + state_.models[i].spin[axis];
            push.legacy_vec_b[axis] = slot.rotation[axis] + state_.models[i].legacy_spin[axis];
        }
        out.push_back(std::move(push));
    }
    EmitUnconditional(current_, Ticks(state_.models), state_.sprite_clock, out);
    return out;
}

void Evaluator::SetOption(int option, int choice) {
    if (document_ == nullptr) return;
    if (option < 0 || (std::size_t)option >= state_.choices.size()) return;
    const Doc::OptionSpec& spec = document_->options[(std::size_t)option];
    if (spec.choices.empty()) return;
    const int clamped = std::clamp(choice, 0, (int)spec.choices.size() - 1);
    const int previous = state_.choices[(std::size_t)option];
    if (clamped == previous) return;

    FrameState resolved = current_;
    ApplyChoiceValues(resolved);
    state_.captured.clear();
    const auto old_index = (std::size_t)std::clamp(previous, 0, (int)spec.choices.size() - 1);
    for (const std::size_t side : {old_index, (std::size_t)clamped}) {
        for (const Doc::ChoiceValue& value : spec.choices[side].values) {
            const bool known =
                std::ranges::any_of(state_.captured, [&value](const CapturedValue& seen) {
                    return seen.id == value.id;
                });
            TweenValue current;
            if (known || !ReadTarget(value.id, resolved, current)) continue;
            state_.captured.push_back(CapturedValue{.id = value.id, .value = current});
        }
    }
    state_.choices[(std::size_t)option] = clamped;
    state_.transition = spec.transition.frames;
    state_.transition_option = option;
    state_.transition_from = previous;

    const float kick = std::max(1.0F, (float)spec.transition.spin_kick);
    const float signed_kick = (clamped > previous) ? kick : -kick;
    for (const Doc::ChoiceValue& value : spec.choices[(std::size_t)clamped].values) {
        if (!value.id.starts_with("model[")) continue;
        const std::size_t close = value.id.find(']');
        const std::string name = value.id.substr(6, close - 6);
        for (std::size_t i = 0; i < current_.models.size() && i < state_.models.size(); i++) {
            if (current_.models[i].name == name) state_.models[i].kick = signed_kick;
        }
    }
    current_ = Resolve(state_.frame);
    ApplyChoiceValues(current_);
    StepEases(current_, false);
}

std::vector<Push> Evaluator::DrawFrame(float dt) const {
    std::vector<Push> out;
    if (document_ == nullptr) return out;
    const int split = current_.sprite_split_priority;
    out.push_back(DrawRange(split, INT_MAX));
    Push behind;
    behind.call = PushCall::DrawParticles;
    CollectParticles(state_.particles, split, true, behind.cells);
    out.push_back(std::move(behind));
    out.push_back(TimedPush(PushCall::RenderFrame, dt));
    out.push_back(DrawRange(INT_MIN, split - 1));
    Push front;
    front.call = PushCall::DrawParticles;
    CollectParticles(state_.particles, split, false, front.cells);
    out.push_back(std::move(front));
    out.push_back(TimedPush(PushCall::AdvanceSprites, dt));
    return out;
}

std::vector<Push> Evaluator::RenderFrame(float dt) {
    std::vector<Push> out = DrawFrame(dt);
    const std::vector<Push> advance = AdvanceFrame();
    out.insert(out.end(), advance.begin(), advance.end());
    return out;
}

void Evaluator::ArmRuntime(const FrameState& next, std::vector<char>& first_frame) {
    for (std::size_t i = 0; i < next.models.size() && i < state_.models.size(); i++) {
        ModelRuntime& runtime = state_.models[i];
        if (next.models[i].draw_start != runtime.draw_start) {
            runtime.draw_start = next.models[i].draw_start;
            runtime.spin = {0.0F, 0.0F, 0.0F};
            runtime.legacy_spin = {0.0F, 0.0F, 0.0F};
            first_frame[i] = 1;
        }
        if (next.models[i].motion_start == runtime.motion_start) continue;
        runtime.motion_start = next.models[i].motion_start;
        if (runtime.motion_start >= 0) runtime.kick = std::max(1.0F, next.models[i].spin_kick);
    }
    for (std::size_t i = 0; i < next.sprites.size() && i < state_.sprite_start.size(); i++) {
        if (next.sprites[i].draw_start == state_.sprite_start[i]) continue;
        state_.sprite_start[i] = next.sprites[i].draw_start;
        if (next.sprites[i].restart_clock) state_.sprite_clock[i] = (float)next.sprites[i].offset;
    }
}

void Evaluator::StepEases(FrameState& state, bool advance) {
    StepCameraEase(state.camera, state_.camera_ease, advance);
    StepCameraMotion(state.camera, state_.camera_motion, advance);
    for (std::size_t i = 0; i < state.models.size() && i < state_.models.size(); i++)
        StepModelEase(state.models[i], (int)i, state_.models[i].ease, state.writes, advance);
}

void Evaluator::EmitChoiceCamera(const FrameState& state, std::vector<Push>& out) const {
    if (document_ == nullptr || !MovesCamera(*document_, state_.choices)) return;
    out.push_back(ViewPush(state.camera, true));
}

void Evaluator::EmitTransforms(const FrameState& state, const std::vector<char>& first_frame,
                               std::vector<Push>& out) {
    const bool chooses = document_ != nullptr && !document_->options.empty();
    for (std::size_t i = 0; i < state.models.size() && i < state_.models.size(); i++) {
        const ModelSlot& slot = state.models[i];
        ModelRuntime& runtime = state_.models[i];
        if (runtime.kick > 1.0F) {
            runtime.kick = std::max(1.0F, runtime.kick - slot.spin_kick_decay);
        } else if (runtime.kick < -1.0F) {
            runtime.kick = std::min(-1.0F, runtime.kick + slot.spin_kick_decay);
        }
        for (std::size_t axis = 0; axis < runtime.spin.size(); axis++) {
            const float step = slot.spin_per_frame[axis] * runtime.kick;
            if (first_frame[i] == 0) runtime.spin[axis] += step;
            runtime.legacy_spin[axis] += step;
        }
        if (!slot.visible) continue;

        const bool shaken = ShakenBy(state.jitter, slot.name);
        const Vec3f position = PlacedPosition(slot, state.jitter, state_.jitter, state.frame);
        Push push = VectorPush(PushCall::SetModelTransform, slot.name, position, true);
        push.model_moves = ModelMoves(slot, shaken, chooses && i == 0);
        for (std::size_t axis = 0; axis < push.vec_b.size(); axis++) {
            push.vec_b[axis] = slot.rotation[axis] + runtime.spin[axis];
            push.legacy_vec_b[axis] = slot.rotation[axis] + runtime.legacy_spin[axis];
        }
        out.push_back(std::move(push));
    }
}

void Evaluator::EmitPulse(const FrameState& state, std::vector<Push>& out) {
    state_.pulse = 1.0F;
    bool active = false;
    for (const ModelSlot& slot : state.models) {
        if (!slot.has_pulse || !PulseActive(state.beat, slot.pulse)) continue;
        state_.pulse = PulseFactor(state.beat, slot.pulse, state_.beat, state_.beat_since);
        active = true;
        break;
    }
    if (!active) return;
    for (const ModelSlot& slot : state.models) {
        const Vec3f scaled = {slot.scale[0] * state_.pulse, slot.scale[1] * state_.pulse,
                              slot.scale[2] * state_.pulse};
        out.push_back(VectorPush(PushCall::SetModelScale, slot.name, scaled, true));
    }
}

void Evaluator::AdvanceBeat(const FrameState& state) {
    if (state.beat.rate <= 0) return;
    const std::array<int, 2> offsets = {state.beat.offset_a, state.beat.offset_b};
    for (std::size_t i = 0; i < offsets.size(); i++) {
        const int index = BeatIndex(state.beat, state.frame, offsets[i]);
        if (index == state_.beat[i]) {
            state_.beat_since[i]++;
            continue;
        }
        state_.beat[i] = index;
        state_.beat_since[i] = 0;
    }
}

void Evaluator::AdvanceClocks(const FrameState& next) {
    for (std::size_t i = 0; i < next.models.size() && i < state_.models.size(); i++) {
        ModelRuntime& runtime = state_.models[i];
        runtime.tick += next.models[i].anim_speed;
        const float max_time = model_max_time_[i];
        if (max_time > 0.0F && runtime.tick > max_time) runtime.tick = 0.0F;
    }
    for (std::size_t i = 0; i < next.sprites.size() && i < state_.sprite_clock.size(); i++)
        state_.sprite_clock[i] += next.sprites[i].speed;
}

std::vector<Push> Evaluator::AdvanceFrame() {
    std::vector<Push> out;
    if (document_ == nullptr) return out;
    const int next = state_.frame + 1;
    const BeatSnapshot snapshot{.index = state_.beat, .since = state_.beat_since};
    AdvanceClocks(current_);

    if (state_.transition > 0 && !document_->options.empty()) state_.transition -= TransitionStep();
    FrameState state = Resolve(next);
    if (FireSelects(state)) state = Resolve(next);
    state.boundary = boundaries_.contains(next);
    std::vector<char> first_frame(state.models.size(), 0);
    ArmRuntime(state, first_frame);

    if (state.boundary) EmitRebind(ResolveWithout(next, false), out);
    AgeParticles(state_.particles);
    EmitSpriteScales(state, out);
    SpawnParticles(state, next, snapshot, document_->render.width, document_->render.height,
                   state_.rng, state_.particles);
    state_.jitter = DrawJitter(state.jitter, state_.rng);

    ApplyChoiceValues(state);
    StepEases(state, true);
    EmitChoiceCamera(state, out);
    EmitTransforms(state, first_frame, out);
    EmitPulse(state, out);
    EmitWrites(state, out);
    AdvanceBeat(state);
    EmitUnconditional(state, Ticks(state_.models), state_.sprite_clock, out);

    for (const Doc::Clip* clip : state.seeds) {
        state_.rng.Seed(std::get<Doc::RngSeed>(clip->command).seed);
        state_.particles.clear();
    }
    state_.frame = next;
    current_ = std::move(state);
    if (next % kCheckpointStride == 0) checkpoints_[next] = state_;
    return out;
}

}
