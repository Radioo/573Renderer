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

struct Point {
    int x = 0;
    int y = 0;

    friend bool operator==(const Point&, const Point&) = default;
};

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    friend bool operator==(const Rect&, const Rect&) = default;
};

struct RectF {
    float x0 = 0.0F;
    float y0 = 0.0F;
    float x1 = 0.0F;
    float y1 = 0.0F;

    friend bool operator==(const RectF&, const RectF&) = default;
};

bool IsFourThree(int w, int h);

Size Present(int render_w, int render_h, bool stretch);

Point ClientToFrame(int client_x, int client_y, Size client, Size frame);

RectF FrameToTarget(const Rect& crop, Size frame, Size target);

const char* FilterName(Filter filter);

std::span<const Filter> AllFilters();

}
