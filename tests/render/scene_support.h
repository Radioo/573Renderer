#pragma once

#include "render_backend.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace SceneSupport {

struct GeoBlob {
    std::array<uint32_t, 16> words{};
};

inline uint32_t F32Bits(float v) {
    return std::bit_cast<uint32_t>(v);
}

inline GeoBlob MakeGeo(uint32_t prim, uint32_t flags, uint32_t tex_ref,
                       const std::vector<float>& tail) {
    GeoBlob g;
    g.words.at(0) = prim;
    g.words.at(1) = flags;
    g.words.at(2) = tex_ref;
    g.words.at(3) = 0;
    for (std::size_t i = 0; i < tail.size(); i++) {
        g.words.at(4 + i) = F32Bits(tail.at(i));
    }
    return g;
}

inline std::vector<float> ColoredQuadStream(float x0, float y0, float x1, float y1,
                                            const std::array<uint32_t, 4>& colors) {
    std::vector<float> src;
    const std::array<float, 4> xs = {x0, x1, x0, x1};
    const std::array<float, 4> ys = {y0, y0, y1, y1};
    const std::array<float, 4> us = {0.0F, 1.0F, 0.0F, 1.0F};
    const std::array<float, 4> vs = {0.0F, 0.0F, 1.0F, 1.0F};
    for (std::size_t v = 0; v < 4; v++) {
        src.push_back(us.at(v));
        src.push_back(vs.at(v));
        src.push_back(std::bit_cast<float>(colors.at(v)));
        src.push_back(xs.at(v));
        src.push_back(ys.at(v));
    }
    return src;
}

inline void SubmitColoredQuad(float x0, float y0, float x1, float y1,
                              const std::array<uint32_t, 4>& colors, uint32_t tex_ref) {
    std::vector<float> src = ColoredQuadStream(x0, y0, x1, y1, colors);
    GeoBlob geo = MakeGeo(5, 0x08 | 0x04 | 0x01, tex_ref, {1.0F, 1.0F, 1.0F, 1.0F});
    AfpD3D9::SubmitGeometry(src.data(), 4, geo.words.data(), nullptr);
}

inline void SubmitColoredPrim(uint32_t prim, const std::vector<float>& xy, uint32_t color) {
    std::vector<float> src;
    for (std::size_t i = 0; i + 1 < xy.size(); i += 2) {
        src.push_back(std::bit_cast<float>(color));
        src.push_back(xy.at(i));
        src.push_back(xy.at(i + 1));
    }
    GeoBlob geo = MakeGeo(prim, 0x04 | 0x01, 0, {0.0F, 0.0F, 1.0F, 1.0F});
    AfpD3D9::SubmitGeometry(src.data(), (int)(xy.size() / 2), geo.words.data(), nullptr);
}

inline void SubmitXyzSkipQuad(float x0, float y0, float x1, float y1, uint32_t color, float z) {
    std::vector<float> src;
    const std::array<float, 4> xs = {x0, x1, x0, x1};
    const std::array<float, 4> ys = {y0, y0, y1, y1};
    const std::array<float, 4> us = {0.0F, 1.0F, 0.0F, 1.0F};
    const std::array<float, 4> vs = {0.0F, 0.0F, 1.0F, 1.0F};
    for (std::size_t v = 0; v < 4; v++) {
        src.push_back(us.at(v));
        src.push_back(vs.at(v));
        src.push_back(7.5F);
        src.push_back(-7.5F);
        src.push_back(std::bit_cast<float>(color));
        src.push_back(xs.at(v));
        src.push_back(ys.at(v));
        src.push_back(z);
    }
    GeoBlob geo = MakeGeo(5, 0x08 | 0x10 | 0x04 | 0x02, 0, {1.0F, 1.0F, 1.0F, 1.0F});
    AfpD3D9::SubmitGeometry(src.data(), 4, geo.words.data(), nullptr);
}

inline void DriveWideScene(uint32_t tex_ref) {
    AfpD3D9::SetLayer(0, 0, nullptr);

    AfpD3D9::SetBlend(2, 0, nullptr);
    SubmitColoredQuad(10.0F, 10.0F, 130.0F, 90.0F, {0xFFFFE0C0, 0xFFC0FFE0, 0xFFE0C0FF, 0xFF808080},
                      0);

    AfpD3D9::SetBlend(3, 0, nullptr);
    SubmitColoredQuad(60.0F, 40.0F, 180.0F, 120.0F,
                      {0xFF400000, 0xFF004000, 0xFF000040, 0xFF404040}, 0);

    AfpD3D9::SetBlend(4, 0, nullptr);
    SubmitColoredQuad(140.0F, 10.0F, 240.0F, 70.0F,
                      {0xFF202020, 0xFF404040, 0xFF404040, 0xFF808080}, 0);

    AfpD3D9::SetBlend(0, 0, nullptr);
    SubmitColoredPrim(1, {20.0F, 150.0F, 120.0F, 150.0F, 20.0F, 160.0F, 120.0F, 200.0F},
                      0xFFFFFF00);
    SubmitColoredPrim(2, {140.0F, 150.0F, 200.0F, 150.0F, 200.0F, 210.0F, 140.0F, 210.0F},
                      0xFF00FFFF);
    SubmitColoredPrim(
        6, {270.0F, 170.0F, 310.0F, 150.0F, 310.0F, 200.0F, 270.0F, 220.0F, 240.0F, 190.0F},
        0xFFFF00FF);

    SubmitXyzSkipQuad(230.0F, 90.0F, 300.0F, 140.0F, 0xA0FFFFFF, 0.25F);

    AfpD3D9::SetLayer(3, 0, nullptr);
    SubmitColoredQuad(20.0F, 60.0F, 100.0F, 140.0F,
                      {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}, tex_ref);
    AfpD3D9::SetLayer(0, 0, nullptr);
}

}
