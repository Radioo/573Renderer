#include <catch2/catch_test_macros.hpp>

#include "render/command_list.h"
#include "render/command_trace.h"
#include "render/hsv_filter.h"
#include "render/vertex_math.h"

#include <array>
#include <cstring>
#include <string>
#include <vector>

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

TEST_CASE("FormatCommand dispatches each variant to its Trace formatter") {
    CHECK(Render::FormatCommand(Render::SetBlendCmd{.mode = 4, .flags = 1}) ==
          Render::TraceSetBlend(4, 1));
    CHECK(Render::FormatCommand(
              Render::MaskCmd{.op = 1, .layer = 2, .x = -4, .y = 8, .w = 640, .h = 480}) ==
          Render::TraceMaskRegion(1, 2, -4, 8, 640, 480));
    CHECK(Render::FormatCommand(Render::LayerCmdCmd{.cmd = 0x1AF, .sub = 5}) ==
          Render::TraceLayerCommand(0x1AF, 5));
    const Render::HsvDescriptor d = Desc(100, 1, 90.0F, -25.0F, 0.0F);
    CHECK(Render::FormatCommand(Render::SetLayerCmd{
              .blend_mode = 0, .has_desc = true, .desc = d}) == Render::TraceSetLayer(0, true, d));
}

TEST_CASE("DrawCmd carries owned verts and formats identically to the raw call") {
    std::vector<AfpVertex> verts(3);
    verts.at(0) = {.x = 539.5F, .y = 100.5F, .z = 0, .color = 0x80FF4020, .u = 0.25F, .v = 0.5F};
    verts.at(1) = {.x = 100.5F, .y = 539.5F, .z = 0, .color = 0xFFFFFFFF, .u = 0.75F, .v = 0.125F};
    verts.at(2) = {.x = 320.5F, .y = 320.5F, .z = 0, .color = 0xFFFFFFFF, .u = 0.5F, .v = 0.25F};
    const std::string raw = Render::TraceDraw(4, 7, false, true, verts);
    const Render::DrawCmd cmd{
        .prim_type = 4, .tex_slot = 7, .hsl = false, .add = true, .verts = verts};
    CHECK(Render::FormatCommand(cmd) == raw);
}

TEST_CASE("FormatCommandList concatenates in record order") {
    Render::RenderCommandList list;
    list.emplace_back(Render::SetBlendCmd{.mode = 0, .flags = 0});
    list.emplace_back(Render::LayerCmdCmd{.cmd = 0x3, .sub = 1});
    CHECK(Render::FormatCommandList(list) ==
          Render::TraceSetBlend(0, 0) + Render::TraceLayerCommand(0x3, 1));
}
