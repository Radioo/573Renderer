#pragma once

#include "preset/doc/preset_document.h"
#include "preset/eval/frame_state.h"

#include <vector>

namespace Preset::Eval {

struct CameraMotion {
    Vec3f up = {0.0F, 1.0F, 0.0F};
    bool armed = false;
};

void StepCameraMotion(CameraState& camera, CameraMotion& runtime, bool advance);

CameraState CameraFrom(const Doc::CameraSpec& spec, int render_w, int render_h);

void ApplyCameraSet(const Doc::Clip& clip, const Doc::CameraSet& command, int frame,
                    CameraState& camera, std::vector<MaterialWrite>& writes, bool with_keys);

void ApplyCameraTween(const Doc::Clip& clip, int frame, CameraState& camera,
                      std::vector<MaterialWrite>& writes);

void ApplyLightSet(const Doc::Clip& clip, const Doc::LightSet& command,
                   std::vector<LightState>& lights);

}
