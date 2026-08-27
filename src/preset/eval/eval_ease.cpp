#include "preset/eval/eval_ease.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/eval/frame_state.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace Preset::Eval {

namespace {

Vec3f ToVec3f(const Doc::Vec3& value) {
    return {(float)value[0], (float)value[1], (float)value[2]};
}

}

float EaseToward(float value, float target, float rate, Doc::EaseMode mode) {
    if (mode == Doc::EaseMode::Geometric) return value + ((target - value) * rate);
    if (value < target) return std::min(target, value + rate);
    return std::max(target, value - rate);
}

void StepCameraEase(CameraState& camera, CameraEase& runtime, bool advance) {
    const Doc::Clip* clip = camera.from.ease;
    if (clip == nullptr) return;
    const auto& command = std::get<Doc::CameraEaseCmd>(clip->command);
    const Vec3f eye_target = ToVec3f(command.eye_target);
    const Vec3f at_target = ToVec3f(command.at_target);
    if (!runtime.armed) {
        runtime.eye = command.start_at_target ? eye_target : camera.eye;
        runtime.at = command.start_at_target ? at_target : camera.at;
        runtime.armed = true;
    }
    const std::array<bool, 3> eye_axes = {command.eye_x, command.eye_y, command.eye_z};
    const std::array<bool, 3> at_axes = {command.at_x, command.at_y, command.at_z};
    const auto rate = (float)command.rate;
    for (std::size_t axis = 0; axis < eye_axes.size(); axis++) {
        if (eye_axes[axis]) {
            if (advance) {
                runtime.eye[axis] =
                    EaseToward(runtime.eye[axis], eye_target[axis], rate, Doc::EaseMode::Geometric);
            }
            camera.eye[axis] = runtime.eye[axis];
        }
        if (!at_axes[axis]) continue;
        if (advance) {
            runtime.at[axis] =
                EaseToward(runtime.at[axis], at_target[axis], rate, Doc::EaseMode::Geometric);
        }
        camera.at[axis] = runtime.at[axis];
    }
    camera.from.eye = clip;
    camera.from.at = clip;
}

void StepModelEase(ModelSlot& slot, int index, ModelEase& runtime,
                   std::vector<MaterialWrite>& writes, bool advance) {
    const Doc::Clip* clip = slot.from.ease;
    if (clip == nullptr) return;
    const auto& command = std::get<Doc::ModelEaseCmd>(clip->command);
    const auto rate = (float)command.rate;
    if (!runtime.armed) {
        const bool seed = command.start_at_target;
        runtime.scale = (seed && command.scale_target.has_value()) ? ToVec3f(*command.scale_target)
                                                                   : slot.scale;
        runtime.position = (seed && command.position_target.has_value())
                               ? ToVec3f(*command.position_target)
                               : slot.position;
        runtime.alpha =
            (seed && command.alpha_target.has_value()) ? (float)*command.alpha_target : slot.alpha;
        runtime.armed = true;
    }
    if (command.scale_target.has_value()) {
        const Vec3f target = ToVec3f(*command.scale_target);
        for (std::size_t axis = 0; advance && axis < runtime.scale.size(); axis++)
            runtime.scale[axis] = EaseToward(runtime.scale[axis], target[axis], rate, command.mode);
        slot.scale = runtime.scale;
        slot.from.scale = clip;
        if (index >= 0) {
            writes.push_back(
                MaterialWrite{.kind = WriteKind::ModelScale, .model = index, .vector = slot.scale});
        }
    }
    if (command.position_target.has_value()) {
        const Vec3f target = ToVec3f(*command.position_target);
        for (std::size_t axis = 0; advance && axis < runtime.position.size(); axis++) {
            runtime.position[axis] =
                EaseToward(runtime.position[axis], target[axis], rate, command.mode);
        }
        slot.position = runtime.position;
        slot.from.position = clip;
    }
    if (!command.alpha_target.has_value()) return;
    if (advance) {
        runtime.alpha = EaseToward(runtime.alpha, (float)*command.alpha_target, rate, command.mode);
    }
    slot.alpha = runtime.alpha;
    slot.from.alpha = clip;
}

}
