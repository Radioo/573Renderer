#include "formats/hsl_adjust.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Hsl {

namespace {

uint32_t ClampChannel(float x) {
    const long n = std::clamp(std::lroundf(x * 255.0F), 0L, 255L);
    return static_cast<uint32_t>(n);
}

}

namespace {

struct Hsl {
    float h;
    float s;
    float l;
};

Hsl RgbToHsl(float r, float g, float b) {
    const float mx = std::max({r, g, b});
    const float mn = std::min({r, g, b});
    const float d = mx - mn;
    const float lum = 0.5F * (mx + mn);
    float h = 0.0F;
    float s = 0.0F;
    if (d > 1e-6F) {
        s = (lum < 0.5F) ? d / (mx + mn) : d / (2.0F - mx - mn);
        if (mx == r) {
            h = ((g - b) / d) + (g < b ? 6.0F : 0.0F);
        } else if (mx == g) {
            h = ((b - r) / d) + 2.0F;
        } else {
            h = ((r - g) / d) + 4.0F;
        }
        h /= 6.0F;
    }
    return {.h = h, .s = s, .l = lum};
}

void HslToRgb(const Hsl& c, float& rr, float& gg, float& bb) {
    const float chroma = (1.0F - std::fabsf((2.0F * c.l) - 1.0F)) * c.s;
    const float hh = c.h * 6.0F;
    const float x = chroma * (1.0F - std::fabsf(std::fmodf(hh, 2.0F) - 1.0F));
    const float m = c.l - (0.5F * chroma);
    rr = 0.0F;
    gg = 0.0F;
    bb = 0.0F;
    switch (static_cast<int>(hh) % 6) {
    case 0:
        rr = chroma;
        gg = x;
        break;
    case 1:
        rr = x;
        gg = chroma;
        break;
    case 2:
        gg = chroma;
        bb = x;
        break;
    case 3:
        gg = x;
        bb = chroma;
        break;
    case 4:
        rr = x;
        bb = chroma;
        break;
    default:
        rr = chroma;
        bb = x;
        break;
    }
    rr += m;
    gg += m;
    bb += m;
}

}

uint32_t AdjustArgb(uint32_t argb, float dh, float ds, float dl) {
    const auto a = static_cast<float>((argb >> 24) & 0xFF);
    const float r = static_cast<float>((argb >> 16) & 0xFF) / 255.0F;
    const float g = static_cast<float>((argb >> 8) & 0xFF) / 255.0F;
    const float b = static_cast<float>(argb & 0xFF) / 255.0F;
    Hsl c = RgbToHsl(r, g, b);
    c.h += dh;
    c.h -= std::floorf(c.h);
    c.s = std::clamp(c.s + ds, 0.0F, 1.0F);
    c.l = std::clamp(c.l + dl, 0.0F, 1.0F);
    float rr = 0.0F;
    float gg = 0.0F;
    float bb = 0.0F;
    HslToRgb(c, rr, gg, bb);
    return (static_cast<uint32_t>(a) << 24) | (ClampChannel(rr) << 16) | (ClampChannel(gg) << 8) |
           ClampChannel(bb);
}

}
