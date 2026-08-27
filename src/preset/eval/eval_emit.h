#pragma once

#include "preset/eval/eval_push.h"
#include "preset/eval/frame_state.h"

#include <string>
#include <vector>

namespace Preset::Eval {

Push ScalarPush(PushCall call, const std::string& name, float value, bool legacy);

Push IntegerPush(PushCall call, const std::string& name, int index, bool legacy);

Push VectorPush(PushCall call, const std::string& name, const Vec3f& value, bool legacy);

Push ViewPush(const CameraState& camera, bool legacy);

Push ProjectionOf(const CameraState& camera, bool legacy);

void EmitRebind(const FrameState& state, std::vector<Push>& out);

void EmitSpriteScales(const FrameState& state, std::vector<Push>& out);

void EmitWrites(const FrameState& state, std::vector<Push>& out);

void EmitUnconditional(const FrameState& state, const std::vector<float>& ticks,
                       const std::vector<float>& clocks, std::vector<Push>& out);

}
