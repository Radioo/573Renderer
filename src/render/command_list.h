#pragma once

#include <array>
#include <string>
#include <variant>
#include <vector>

#include "render/hsv_filter.h"
#include "render/vertex_math.h"

namespace Render {

struct SetLayerCmd {
    unsigned int blend_mode = 0;
    bool has_desc = false;
    HsvDescriptor desc;
};

struct SetBlendCmd {
    unsigned int mode = 0;
    unsigned int flags = 0;
};

struct DrawCmd {
    int prim_type = 0;
    int tex_slot = -1;
    bool hsl = false;
    bool add = false;
    bool matrix_ready = false;
    std::vector<AfpVertex> verts;
    std::array<float, 4> shape_color = {1.0F, 1.0F, 1.0F, 1.0F};
    std::array<float, 4> hsv_c1 = {};
    std::array<float, 4> hsv_c3 = {};
    std::array<float, 16> matrix = {};
};

struct MaskCmd {
    unsigned int op = 0;
    unsigned int layer = 0;
    int x = 0;
    int y = 0;
    unsigned int w = 0;
    unsigned int h = 0;
};

struct LayerCmdCmd {
    unsigned int cmd = 0;
    unsigned int sub = 0;
};

using RenderCommand = std::variant<SetLayerCmd, SetBlendCmd, DrawCmd, MaskCmd, LayerCmdCmd>;
using RenderCommandList = std::vector<RenderCommand>;

[[nodiscard]] std::string FormatCommand(const RenderCommand& cmd);

[[nodiscard]] std::string FormatCommandList(const RenderCommandList& list);

}
