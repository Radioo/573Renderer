#pragma once

#include "formats/sysidx.h"

#include <cstdint>

#include <vector>

namespace GcAnim {

enum class Blend : uint8_t {
    Normal,
    Additive,
    Subtract,
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

void Evaluate(const SysIdx::Package& pkg, size_t start_index, int frame, float ox, float oy,
              std::vector<DrawNode>& out);

}
