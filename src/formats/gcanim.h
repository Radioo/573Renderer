#pragma once

#include "formats/sysidx.h"

#include <cstdint>

#include <span>
#include <vector>

namespace GcAnim {

enum class Blend : uint8_t {
    Normal,
    Additive,
    Subtract,
};

enum class Playback : uint8_t {
    Loop,
    HoldLast,
    HideAfterEnd,
};

struct Timing {
    Playback playback = Playback::Loop;
    int loop_start = 0;
    int loop_end = 0;
};

struct DrawNode {
    int cell = -1;
    float x = 0.0F;
    float y = 0.0F;
    float w = 0.0F;
    float h = 0.0F;
    float rotation = 0.0F;
    float pivot_x = 0.0F;
    float pivot_y = 0.0F;
    float alpha = 1.0F;
    Blend blend = Blend::Normal;
};

int SampleTrack(const std::vector<SysIdx::Key>& keys, int t, int fallback_a, int& out_b);

[[nodiscard]] int ResolveFrame(int time, int length, const Timing& timing);

struct SkipSet {
    std::span<const size_t> children;
    std::span<const int> cells;
};

void Evaluate(const SysIdx::Package& pkg, size_t start_index, int frame, float ox, float oy,
              std::vector<DrawNode>& out, const SkipSet& skip = {});

}
