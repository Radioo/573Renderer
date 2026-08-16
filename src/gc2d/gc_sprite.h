#pragma once

#include "formats/gcanim.h"
#include "formats/sysidx.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace Gc2d {

struct Canvas {
    int width = 640;
    int height = 480;
};

struct SpriteDraw {
    std::string name;
    bool animated = false;
    float x = 0.0F;
    float y = 0.0F;
    float alpha = 1.0F;
    float scale = 1.0F;
    GcAnim::Blend blend = GcAnim::Blend::Normal;
    GcAnim::Timing timing = {};
    std::vector<std::string> skip_parts;
    float time = 0.0F;
    float scroll_x = 0.0F;
    float scroll_wrap = 0.0F;
    float scroll_offset = 0.0F;
};

std::array<float, 2> ScaleFactors(const Canvas& canvas, int target_width, int target_height);

std::array<float, 2> PivotFor(const Canvas& canvas, float x, float y);

float ScrollOffset(const SpriteDraw& sprite);

int SpriteLength(const SysIdx::Package& index, const SpriteDraw& sprite);

std::vector<std::string> PartNames(const SysIdx::Package& index, const std::string& animation);

void AppendNodes(const SysIdx::Package& index, const SpriteDraw& sprite, const Canvas& canvas,
                 std::vector<GcAnim::DrawNode>& out);

}
