#pragma once

#include "preset/doc/preset_document.h"
#include "preset/eval/eval_tween.h"
#include "preset/eval/frame_state.h"

#include <array>
#include <string_view>

namespace Preset::Eval {

int BeatIndex(const BeatState& beat, int frame, int offset);

bool PulseActive(const BeatState& beat, const PulseState& pulse);

float PulseFactor(const BeatState& beat, const PulseState& pulse, const std::array<int, 2>& indices,
                  const std::array<int, 2>& since);

bool ReadTarget(std::string_view id, const FrameState& state, TweenValue& out);

bool WriteTarget(std::string_view id, const TweenValue& value, FrameState& state);

TweenValue OverrideToTween(const Doc::OverrideValue& value);

}
