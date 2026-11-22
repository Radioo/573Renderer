#pragma once

#include "judge/afp_pose.h"
#include "judge/interp_layout.h"
#include "judge/interp_tracks.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace AfpInterpHooks {

struct LayerStats {
    std::size_t moving = 0;
    std::size_t swapped = 0;
    float alpha = 0.0F;
};

using AdvanceObserver = void (*)(std::span<const AfpInterp::Sample> before,
                                 std::span<const AfpInterp::Sample> after, int steps);

[[nodiscard]] std::string Install(std::uintptr_t libafp_base, const AfpInterp::Layout& layout);
void Uninstall();
[[nodiscard]] bool Installed();

void SetLayerInterpolated(std::uint32_t layer_id, bool on);
void SetMatrixLerp(AfpPose::MatrixLerp mode);
void SetRenderTime(float seconds_since_advance);
void SetAdvanceObserver(AdvanceObserver observer);

[[nodiscard]] LayerStats StatsFor(std::uint32_t layer_id);

}
