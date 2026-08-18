#include "preset/eval/eval_camera_lights.h"

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/eval/eval_tween.h"
#include "preset/eval/frame_state.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace Preset::Eval {

namespace {

Vec3f ToVec3f(const Doc::Vec3& value) {
    return {(float)value[0], (float)value[1], (float)value[2]};
}

TweenValue ScalarOf(float value) {
    TweenValue out;
    out.kind = TweenValue::Kind::Scalar;
    out.scalar = value;
    return out;
}

TweenValue VectorOf(const Vec3f& value) {
    TweenValue out;
    out.kind = TweenValue::Kind::Vector;
    out.vector = value;
    return out;
}

void RecordWrite(std::vector<MaterialWrite>& writes, WriteKind kind, bool legacy,
                 const CameraState& camera) {
    writes.push_back(MaterialWrite{.kind = kind, .model = -1, .legacy = legacy, .camera = camera});
}

void SampleCameraKeys(const Doc::Clip& clip, int frame, CameraState& camera,
                      std::vector<MaterialWrite>& writes) {
    if (clip.keys.empty()) return;
    const int clip_frame = frame - clip.start;
    TweenValue sampled;
    if (SampleKeys(clip.keys, "fov_y", clip_frame, ScalarOf(camera.fov_y), sampled)) {
        camera.fov_y = sampled.scalar;
        camera.from.fov_y = &clip;
        RecordWrite(writes, WriteKind::CameraProjection, true, camera);
    }
    if (SampleKeys(clip.keys, "near_z", clip_frame, ScalarOf(camera.near_z), sampled)) {
        camera.near_z = sampled.scalar;
        camera.from.near_z = &clip;
        RecordWrite(writes, WriteKind::CameraProjection, false, camera);
    }
    if (SampleKeys(clip.keys, "far_z", clip_frame, ScalarOf(camera.far_z), sampled)) {
        camera.far_z = sampled.scalar;
        camera.from.far_z = &clip;
        RecordWrite(writes, WriteKind::CameraProjection, false, camera);
    }
    bool moved = false;
    if (SampleKeys(clip.keys, "eye", clip_frame, VectorOf(camera.eye), sampled)) {
        camera.eye = sampled.vector;
        camera.from.eye = &clip;
        moved = true;
    }
    if (SampleKeys(clip.keys, "at", clip_frame, VectorOf(camera.at), sampled)) {
        camera.at = sampled.vector;
        camera.from.at = &clip;
        moved = true;
    }
    if (SampleKeys(clip.keys, "up", clip_frame, VectorOf(camera.up), sampled)) {
        camera.up = sampled.vector;
        camera.from.up = &clip;
        moved = true;
    }
    if (moved) RecordWrite(writes, WriteKind::CameraView, false, camera);
}

}

CameraState CameraFrom(const Doc::CameraSpec& spec, int render_w, int render_h) {
    CameraState out;
    out.eye = ToVec3f(spec.eye);
    out.at = ToVec3f(spec.at);
    out.up = ToVec3f(spec.up);
    out.fov_y = (float)spec.fov_y;
    out.near_z = (float)spec.near_z;
    out.far_z = (float)spec.far_z;
    out.aspect_auto = spec.aspect.automatic;
    out.aspect_value =
        out.aspect_auto ? ((float)render_w / (float)render_h) : (float)spec.aspect.value;
    return out;
}

void ApplyCameraSet(const Doc::Clip& clip, const Doc::CameraSet& command, int frame,
                    CameraState& camera, std::vector<MaterialWrite>& writes, bool with_keys) {
    if (command.eye.has_value()) {
        camera.eye = ToVec3f(*command.eye);
        camera.from.eye = &clip;
    }
    if (command.at.has_value()) {
        camera.at = ToVec3f(*command.at);
        camera.from.at = &clip;
    }
    if (command.up.has_value()) {
        camera.up = ToVec3f(*command.up);
        camera.from.up = &clip;
    }
    if (command.fov_y.has_value()) {
        camera.fov_y = (float)*command.fov_y;
        camera.from.fov_y = &clip;
    }
    if (command.near_z.has_value()) {
        camera.near_z = (float)*command.near_z;
        camera.from.near_z = &clip;
    }
    if (command.far_z.has_value()) {
        camera.far_z = (float)*command.far_z;
        camera.from.far_z = &clip;
    }
    if (command.aspect.has_value()) {
        camera.aspect_auto = command.aspect->automatic;
        if (!camera.aspect_auto) camera.aspect_value = (float)command.aspect->value;
        camera.from.aspect = &clip;
    }
    if (with_keys) SampleCameraKeys(clip, frame, camera, writes);
}

void ApplyCameraTween(const Doc::Clip& clip, int frame, CameraState& camera,
                      std::vector<MaterialWrite>& writes) {
    SampleCameraKeys(clip, frame, camera, writes);
}

void ApplyLightSet(const Doc::Clip& clip, const Doc::LightSet& command,
                   std::vector<LightState>& lights) {
    const auto index = (std::size_t)std::max(0, command.index);
    if (index >= lights.size()) lights.resize(index + 1);
    LightState& light = lights[index];
    if (command.direction.has_value()) light.direction = ToVec3f(*command.direction);
    if (command.diffuse.has_value()) light.diffuse = ToVec3f(*command.diffuse);
    if (command.specular.has_value()) light.specular = ToVec3f(*command.specular);
    light.enabled = command.enabled;
    light.from = &clip;
}

}
