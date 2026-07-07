#include <catch2/catch_test_macros.hpp>

#include "render/command_diff.h"
#include "render/command_list.h"
#include "render/vertex_math.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace {

void ExpectDiv(const std::optional<Render::CommandDivergence>& d, size_t index,
               const std::string& reason) {
    REQUIRE(d.has_value());
    if (d.has_value()) {
        CHECK(d->index == index);
        CHECK(d->reason == reason);
    }
}

}

TEST_CASE("DiffCommandLists returns nullopt for equivalent lists") {
    Render::RenderCommandList a;
    a.emplace_back(Render::SetBlendCmd{.mode = 4, .flags = 1});
    a.emplace_back(Render::LayerCmdCmd{.cmd = 0x3, .sub = 1});
    const Render::RenderCommandList b = a;
    CHECK_FALSE(DiffCommandLists(a, b, 0.001F).has_value());
}

TEST_CASE("Integer fields must match exactly") {
    Render::RenderCommandList a;
    a.emplace_back(Render::SetBlendCmd{.mode = 4, .flags = 1});
    Render::RenderCommandList b;
    b.emplace_back(Render::SetBlendCmd{.mode = 8, .flags = 1});
    ExpectDiv(DiffCommandLists(a, b, 0.001F), 0, "mode 4 != 8");
}

TEST_CASE("Float fields tolerate epsilon but flag larger drift") {
    const auto layer = [](float hue) {
        Render::SetLayerCmd c{.blend_mode = 0, .has_desc = true, .desc = {}};
        c.desc.id = 100;
        c.desc.valid = 1;
        c.desc.hue = hue;
        return c;
    };
    Render::RenderCommandList a;
    a.emplace_back(layer(136.0F));
    Render::RenderCommandList within;
    within.emplace_back(layer(136.0005F));
    CHECK_FALSE(DiffCommandLists(a, within, 0.001F).has_value());
    Render::RenderCommandList beyond;
    beyond.emplace_back(layer(136.5F));
    CHECK(DiffCommandLists(a, beyond, 0.001F).has_value());
}

TEST_CASE("Vertex positions compare within epsilon, colors exactly") {
    const auto draw = [](float x, unsigned int color) {
        std::vector<AfpVertex> v(1);
        v.at(0) = {.x = x, .y = 0, .z = 0, .color = color, .u = 0, .v = 0};
        return Render::DrawCmd{
            .prim_type = 4, .tex_slot = 1, .hsl = false, .add = false, .verts = v};
    };
    Render::RenderCommandList a;
    a.emplace_back(draw(100.0F, 0xFFFFFFFF));
    Render::RenderCommandList near_pos;
    near_pos.emplace_back(draw(100.0002F, 0xFFFFFFFF));
    CHECK_FALSE(DiffCommandLists(a, near_pos, 0.001F).has_value());
    Render::RenderCommandList bad_color;
    bad_color.emplace_back(draw(100.0F, 0xFF000000));
    ExpectDiv(DiffCommandLists(a, bad_color, 0.001F), 0, "vert 0 color FFFFFFFF != FF000000");
}

TEST_CASE("Command type mismatch and length mismatch are reported") {
    Render::RenderCommandList a;
    a.emplace_back(Render::SetBlendCmd{.mode = 0, .flags = 0});
    Render::RenderCommandList typed;
    typed.emplace_back(Render::MaskCmd{.op = 0, .layer = 0, .x = 0, .y = 0, .w = 0, .h = 0});
    ExpectDiv(DiffCommandLists(a, typed, 0.001F), 0, "command type differs");

    Render::RenderCommandList longer = a;
    longer.emplace_back(Render::SetBlendCmd{.mode = 1, .flags = 0});
    ExpectDiv(DiffCommandLists(a, longer, 0.001F), 1, "length 1 != 2");
}

TEST_CASE("Draw matrices compare within epsilon and matrix_ready exactly") {
    Render::DrawCmd d;
    d.matrix_ready = true;
    d.matrix[3] = 0.5F;
    Render::RenderCommandList a;
    a.emplace_back(d);

    Render::DrawCmd close = d;
    close.matrix[3] = 0.5004F;
    Render::RenderCommandList near_list;
    near_list.emplace_back(close);
    CHECK(!DiffCommandLists(a, near_list, 0.001F).has_value());

    Render::DrawCmd far_cmd = d;
    far_cmd.matrix[3] = 0.6F;
    Render::RenderCommandList far_list;
    far_list.emplace_back(far_cmd);
    ExpectDiv(DiffCommandLists(a, far_list, 0.001F), 0, "matrix 3 0.5 != 0.6");

    Render::DrawCmd not_ready = d;
    not_ready.matrix_ready = false;
    Render::RenderCommandList nr;
    nr.emplace_back(not_ready);
    ExpectDiv(DiffCommandLists(a, nr, 0.001F), 0, "matrix_ready true != false");
}
