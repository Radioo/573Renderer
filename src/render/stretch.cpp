#include "render/stretch.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace Stretch {

namespace {

constexpr int kWideNumerator = 16;
constexpr int kWideDenominator = 9;

constexpr std::array<Filter, 4> kFilters = {Filter::Nearest, Filter::Linear, Filter::Gaussian,
                                            Filter::Pyramidal};

}

bool IsFourThree(int w, int h) {
    if (w <= 0 || h <= 0) return false;
    return w * 3 == h * 4;
}

Size Present(int render_w, int render_h, bool stretch) {
    if (!stretch || !IsFourThree(render_w, render_h)) {
        return Size{.w = render_w, .h = render_h};
    }
    const int exact = ((render_h * kWideNumerator) + (kWideDenominator / 2)) / kWideDenominator;
    return Size{.w = exact + (exact & 1), .h = render_h};
}

Point ClientToFrame(int client_x, int client_y, Size client, Size frame) {
    const int cw = (client.w > 0) ? client.w : 1;
    const int ch = (client.h > 0) ? client.h : 1;
    const int px = std::clamp(client_x, 0, cw);
    const int py = std::clamp(client_y, 0, ch);
    return Point{.x = (int)(((int64_t)px * frame.w) / cw),
                 .y = (int)(((int64_t)py * frame.h) / ch)};
}

RectF FrameToTarget(const Rect& crop, Size frame, Size target) {
    if (frame.w <= 0 || frame.h <= 0) return RectF{};
    const auto sx = (float)target.w / (float)frame.w;
    const auto sy = (float)target.h / (float)frame.h;
    return RectF{.x0 = (float)crop.x * sx,
                 .y0 = (float)crop.y * sy,
                 .x1 = (float)(crop.x + crop.w) * sx,
                 .y1 = (float)(crop.y + crop.h) * sy};
}

const char* FilterName(Filter filter) {
    switch (filter) {
    case Filter::Nearest:
        return "Nearest";
    case Filter::Linear:
        return "Linear";
    case Filter::Gaussian:
        return "Gaussian";
    case Filter::Pyramidal:
        return "Pyramidal";
    default:
        return "Linear";
    }
}

std::span<const Filter> AllFilters() {
    return kFilters;
}

}
