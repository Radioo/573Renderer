#include <catch2/catch_test_macros.hpp>

#include "render/vertex_math.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

TEST_CASE("SnapHalfPixel lands every coordinate on the real game's half-pixel grid") {
    CHECK(Render::SnapHalfPixel(540.0F) == 539.5F);
    CHECK(Render::SnapHalfPixel(568.5F) == 568.5F);
    CHECK(Render::SnapHalfPixel(1079.5F) == 1079.5F);
    CHECK(Render::SnapHalfPixel(1317.0F) == 1316.5F);
}

TEST_CASE("DecodeAfpVertices unpacks xy plus uv plus color streams") {
    const float ruv = 0.75F;
    float color_bits = 0;
    const uint32_t argb = 0x80FF4020;
    std::memcpy(&color_bits, &argb, sizeof(argb));
    const std::array<float, 5> src = {0.25F, ruv, color_bits, 10.0F, 20.0F};
    const std::array<float, 2> uv = {0.0F, 0.0F};
    std::array<AfpVertex, 1> out{};
    const int used = Render::DecodeAfpVertices(src, uv, 0x08 | 0x04 | 0x01, out);
    CHECK(used == 5);
    CHECK(out.at(0).u == 0.25F);
    CHECK(out.at(0).v == ruv);
    CHECK(out.at(0).color == argb);
    CHECK(out.at(0).x == 9.5F);
    CHECK(out.at(0).y == 19.5F);
    CHECK(out.at(0).z == 0.0F);
}

TEST_CASE("DecodeAfpVertices takes xyz when flag 2 is set and skips flag 0x10 words") {
    const std::array<float, 5> src = {9.9F, 9.9F, 4.0F, 5.5F, 6.0F};
    const std::array<float, 2> uv = {0.125F, 0.25F};
    std::array<AfpVertex, 1> out{};
    const int used = Render::DecodeAfpVertices(src, uv, 0x10 | 0x02, out);
    CHECK(used == 5);
    CHECK(out.at(0).u == 0.125F);
    CHECK(out.at(0).v == 0.25F);
    CHECK(out.at(0).color == 0xFFFFFFFF);
    CHECK(out.at(0).x == 3.5F);
    CHECK(out.at(0).y == 5.5F);
    CHECK(out.at(0).z == 6.0F);
}

namespace {

std::array<AfpVertex, 6> Quad(float umin, float umax, float vmin, float vmax) {
    std::array<AfpVertex, 6> q{};
    const std::array<float, 6> us = {umin, umax, umin, umax, umin, umax};
    const std::array<float, 6> vs = {vmin, vmin, vmax, vmax, vmin, vmax};
    for (std::size_t i = 0; i < q.size(); i++) {
        q.at(i).u = us.at(i);
        q.at(i).v = vs.at(i);
    }
    return q;
}

}

TEST_CASE("InsetQuadUvs pulls the quad's UV extremes a quarter texel inward") {
    std::array<AfpVertex, 6> q = Quad(0.5F, 0.70312F, 0.25F, 0.5F);
    Render::InsetQuadUvs(q, 4, 2048, 1024);
    const float inset_u = 0.25F / 2048.0F;
    const float inset_v = 0.25F / 1024.0F;
    CHECK(q.at(0).u == 0.5F + inset_u);
    CHECK(q.at(1).u == 0.70312F - inset_u);
    CHECK(q.at(0).v == 0.25F + inset_v);
    CHECK(q.at(2).v == 0.5F - inset_v);
}

TEST_CASE("InsetQuadUvs skips degenerate spans and non trianglelist draws") {
    std::array<AfpVertex, 6> tiny = Quad(0.5F, 0.5F + (0.5F / 2048.0F), 0.25F, 0.5F);
    const std::array<AfpVertex, 6> before = tiny;
    Render::InsetQuadUvs(tiny, 4, 2048, 1024);
    CHECK(tiny.at(0).u == before.at(0).u);
    CHECK(tiny.at(1).u == before.at(1).u);
    CHECK(tiny.at(0).v != before.at(0).v);

    std::array<AfpVertex, 6> strip = Quad(0.5F, 0.70312F, 0.25F, 0.5F);
    const std::array<AfpVertex, 6> strip_before = strip;
    Render::InsetQuadUvs(strip, 5, 2048, 1024);
    CHECK(strip.at(0).u == strip_before.at(0).u);
    CHECK(strip.at(2).v == strip_before.at(2).v);
}

TEST_CASE("AfpPrimCount mirrors the bm2dx prim mapping") {
    CHECK(Render::AfpPrimCount(1, 6) == 3);
    CHECK(Render::AfpPrimCount(3, 6) == 3);
    CHECK(Render::AfpPrimCount(2, 6) == 5);
    CHECK(Render::AfpPrimCount(4, 6) == 2);
    CHECK(Render::AfpPrimCount(5, 6) == 4);
    CHECK(Render::AfpPrimCount(6, 6) == 4);
    CHECK(Render::AfpPrimCount(0, 6) == 4);
    CHECK(Render::AfpPrimCount(4, 2) == 0);
}
