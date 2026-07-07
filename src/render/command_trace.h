#pragma once

#include <span>
#include <string>

#include "render/hsv_filter.h"
#include "render/vertex_math.h"

namespace Render {

[[nodiscard]] std::string TraceSetLayer(unsigned int blend_mode, bool has_desc,
                                        const HsvDescriptor& d);

[[nodiscard]] std::string TraceSetBlend(unsigned int mode, unsigned int flags);

[[nodiscard]] std::string TraceDraw(int prim_type, int tex_slot, bool hsl, bool add,
                                    std::span<const AfpVertex> verts);

[[nodiscard]] std::string TraceMaskRegion(unsigned int op, unsigned int layer, int x, int y,
                                          unsigned int w, unsigned int h);

[[nodiscard]] std::string TraceLayerCommand(unsigned int cmd, unsigned int sub);

}
