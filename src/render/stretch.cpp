#include "render/stretch.h"

#include <array>
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
