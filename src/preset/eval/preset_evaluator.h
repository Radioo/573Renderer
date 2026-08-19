#pragma once

#include "preset/doc/preset_document.h"
#include "preset/eval/eval_push.h"
#include "preset/eval/eval_state.h"
#include "preset/eval/frame_state.h"
#include "preset/preset_asset_lengths.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace Preset::Eval {

class Evaluator {
public:
    void Load(std::shared_ptr<const Doc::Document> document, Preset::AssetLengths lengths);

    void Reset();

    std::vector<Push> Seek(int frame);

    void SetOption(int option, int choice);

    std::vector<Push> RenderFrame(float dt);

    [[nodiscard]] std::vector<Push> DrawFrame(float dt) const;

    std::vector<Push> AdvanceFrame();

    [[nodiscard]] std::vector<Push> Rebind() const;

    [[nodiscard]] const EvalState& State() const { return state_; }

    [[nodiscard]] const FrameState& Current() const { return current_; }

    [[nodiscard]] FrameState Resolve(int frame) const;

private:
    [[nodiscard]] int Length() const;
    [[nodiscard]] int DerivedLength() const;
    [[nodiscard]] FrameState ResolveWithout(int frame, bool tweens) const;
    void ResetRuntime();
    bool FireSelects(const FrameState& state);
    [[nodiscard]] int TransitionStep() const;
    void ApplyChoiceValues(FrameState& state) const;
    void ArmRuntime(const FrameState& next, std::vector<char>& first_frame);
    void StepEases(FrameState& state, bool advance);
    void EmitChoiceCamera(const FrameState& state, std::vector<Push>& out) const;
    void EmitTransforms(const FrameState& state, const std::vector<char>& first_frame,
                        std::vector<Push>& out);
    void EmitPulse(const FrameState& state, std::vector<Push>& out);
    void AdvanceBeat(const FrameState& state);
    void AdvanceClocks(const FrameState& next);
    [[nodiscard]] std::string AssetDir(const std::string& asset_id) const;

    std::shared_ptr<const Doc::Document> document_;
    Preset::AssetLengths lengths_;
    FrameState current_;
    EvalState state_;
    std::set<int> boundaries_;
    std::map<int, EvalState> checkpoints_;
    std::vector<float> model_max_time_;
};

}
