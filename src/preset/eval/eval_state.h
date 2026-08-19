#pragma once

#include "preset/eval/eval_camera_lights.h"
#include "preset/eval/eval_ease.h"
#include "preset/eval/eval_particles.h"
#include "preset/eval/eval_tween.h"
#include "preset/eval/frame_state.h"
#include "preset/preset_rng.h"

#include <array>
#include <string>
#include <vector>

namespace Preset::Eval {

struct ModelRuntime {
    Vec3f spin = {0.0F, 0.0F, 0.0F};
    Vec3f legacy_spin = {0.0F, 0.0F, 0.0F};
    float kick = 1.0F;
    float tick = 0.0F;
    int draw_start = -1;
    int motion_start = -1;
    ModelEase ease = {};
};

struct CapturedValue {
    std::string id;
    TweenValue value;
};

struct EvalState {
    int frame = 0;
    Preset::Ran3 rng;
    std::vector<Particle> particles;
    std::array<int, 2> beat = {0, 0};
    std::array<int, 2> beat_since = {0, 0};
    float jitter = 0.0F;
    float pulse = 1.0F;
    std::vector<ModelRuntime> models;
    std::vector<float> sprite_clock;
    std::vector<int> sprite_start;
    int transition = 0;
    int transition_option = -1;
    int transition_from = -1;
    std::vector<int> choices;
    std::vector<CapturedValue> captured;
    CameraEase camera_ease;
    CameraMotion camera_motion;
};

}
