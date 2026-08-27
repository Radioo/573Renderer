#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace Scene3d {

struct MovieReporter {
    std::function<void(const std::string& what)> begin;
    std::function<void(const std::string& stage, float fraction)> stage;
    std::function<void()> end;
};

struct PolyTile {
    std::array<std::array<float, 3>, 4> corners = {};
    std::array<std::array<float, 2>, 4> uv = {};
};

struct PolyGrid {
    bool active = false;
    float alpha = 1.0F;
    float seconds = 0.0F;
    int movie_width = 0;
    int movie_height = 0;
    int texture_side = 0;
    std::string movie;
    std::vector<PolyTile> tiles;
};

}
