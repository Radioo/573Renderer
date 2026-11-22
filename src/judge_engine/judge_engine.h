#pragma once

#include "judge/afp_pose.h"
#include "judge_hooks/afp_interp_hooks.h"

#include <d3d9.h>

#include <cstdint>
#include <string>

namespace JudgeEngine {

struct BootResult {
    std::string error;
    std::string interp_problem;
    float fixed_step_seconds = 0.0F;
};

struct Playhead {
    int frame = -1;
    int total = 0;
};

[[nodiscard]] BootResult Boot(const std::string& game_dir, const std::string& profile_slug,
                              IDirect3DDevice9* device, int stage_w, int stage_h);

[[nodiscard]] std::string OpenClip(const std::string& package_path, const std::string& clip);

void Advance(float dt);
void SetRenderTime(float seconds_since_advance);
void SetMatrixLerp(AfpPose::MatrixLerp mode);

enum class Side : std::uint8_t { Authored, Interpolated };

void DrawSide(Side side);

[[nodiscard]] Playhead ReadPlayhead();
[[nodiscard]] AfpInterpHooks::LayerStats InterpolationStats();

void Shutdown();

}
