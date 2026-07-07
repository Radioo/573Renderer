#include <catch2/catch_test_macros.hpp>

#include "render/command_trace.h"
#include "render/hsv_filter.h"
#include "render/vertex_math.h"

#include <array>
#include <cstring>

namespace {

Render::HsvDescriptor Desc(unsigned char id, unsigned short valid, float hue, float sat,
                           float val) {
    std::array<unsigned char, 16> raw{};
    raw.at(0) = id;
    std::memcpy(&raw.at(2), &valid, sizeof(valid));
    std::memcpy(&raw.at(4), &hue, sizeof(hue));
    std::memcpy(&raw.at(8), &sat, sizeof(sat));
    std::memcpy(&raw.at(12), &val, sizeof(val));
    return Render::ParseHsvDescriptor(raw);
}

}

TEST_CASE("TraceSetLayer renders with and without a descriptor") {
    CHECK(Render::TraceSetLayer(8, false, {}) == "layer mode=8 hsv=-\n");
    CHECK(Render::TraceSetLayer(0, true, Desc(100, 1, 136.0F, -25.0F, 0.5F)) ==
          "layer mode=0 hsv={id=100 valid=1 h=136.00 s=-25.00 v=0.50}\n");
}

TEST_CASE("TraceSetBlend is stable") {
    CHECK(Render::TraceSetBlend(4, 1) == "blend mode=4 flags=1\n");
}

TEST_CASE("TraceDraw reports bboxes at fixed precision") {
    std::array<AfpVertex, 3> verts{};
    verts.at(0) = {.x = 539.5F, .y = 100.5F, .z = 0, .color = 0x80FF4020, .u = 0.25F, .v = 0.5F};
    verts.at(1) = {.x = 100.5F, .y = 539.5F, .z = 0, .color = 0xFFFFFFFF, .u = 0.75F, .v = 0.125F};
    verts.at(2) = {.x = 320.5F, .y = 320.5F, .z = 0, .color = 0xFFFFFFFF, .u = 0.5F, .v = 0.25F};
    CHECK(Render::TraceDraw(4, 7, false, true, verts) ==
          "draw prim=4 vc=3 tex=7 ps=add xy=[100.5,100.5..539.5,539.5] "
          "uv=[0.2500,0.1250..0.7500,0.5000] c0=80FF4020\n");
    CHECK(Render::TraceDraw(5, 0, false, false, {}) == "draw prim=5 empty\n");
}

TEST_CASE("TraceMaskRegion and TraceLayerCommand are stable") {
    CHECK(Render::TraceMaskRegion(1, 2, -4, 8, 640, 480) ==
          "mask op=1 layer=2 rect=[-4,8 640x480]\n");
    CHECK(Render::TraceLayerCommand(0x1AF, 5) == "layercmd cmd=0x1AF sub=5\n");
}
