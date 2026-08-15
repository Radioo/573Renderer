#pragma once

#include "preset/doc/preset_document.h"
#include "preset/eval/frame_state.h"

#include <string>
#include <vector>

namespace Preset::Eval {

void ApplyModelDraw(const Doc::Clip& clip, const Doc::ModelDraw& command, int frame,
                    ModelSlot& slot, int model_index, std::vector<MaterialWrite>& writes,
                    bool with_keys);

void ApplyModelTween(const Doc::Clip& clip, int frame, ModelSlot& slot, int model_index,
                     std::vector<MaterialWrite>& writes);

void ApplyModelMotion(const Doc::Clip& clip, const Doc::ModelMotionCmd& command, ModelSlot& slot);

Vec3f OrbitPosition(const OrbitState& orbit, int frame);

bool ShakenBy(const JitterState& jitter, const std::string& model);

Vec3f PlacedPosition(const ModelSlot& slot, const JitterState& jitter, float shake, int frame);

bool ModelMoves(const ModelSlot& slot, bool jitter_active, bool choice_driven);

}
