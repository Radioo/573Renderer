#pragma once

#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/frame_state.h"

#include <vector>

namespace Preset::Eval {

struct CameraEase {
    Vec3f eye = {0.0F, 0.0F, 0.0F};
    Vec3f at = {0.0F, 0.0F, 0.0F};
    bool armed = false;
};

struct ModelEase {
    Vec3f scale = {1.0F, 1.0F, 1.0F};
    Vec3f position = {0.0F, 0.0F, 0.0F};
    float alpha = 1.0F;
    bool armed = false;
};

float EaseToward(float value, float target, float rate, Doc::EaseMode mode);

void StepCameraEase(CameraState& camera, CameraEase& runtime, bool advance);

void StepModelEase(ModelSlot& slot, int index, ModelEase& runtime,
                   std::vector<MaterialWrite>& writes, bool advance);

}
