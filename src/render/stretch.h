#pragma once

#include <cstdint>
#include <span>

namespace Stretch {

enum class Filter : uint8_t {
    Nearest,
    Linear,
    Gaussian,
    Pyramidal,
};

struct Size {
    int w = 0;
    int h = 0;

    friend bool operator==(const Size&, const Size&) = default;
};

bool IsFourThree(int w, int h);

Size Present(int render_w, int render_h, bool stretch);

const char* FilterName(Filter filter);

std::span<const Filter> AllFilters();

}
