#pragma once

#include <array>
#include <string>

namespace Gc2dSheet {

struct Job {
    std::string package_dir;
    std::string out_dir;
    int samples = 0;
    std::array<float, 2> at = {0.0F, 0.0F};
    bool straight_alpha = false;
    std::string parts_dir;
    std::string playfield;
};

int Run(const Job& job);

}
