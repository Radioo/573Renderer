#pragma once

#include <array>
#include <cstddef>

namespace Preset {

class Ran3 {
public:
    void Seed(int seed);

    int Next();

private:
    void Refill();

    std::array<int, 56> table_ = {};
    int index_ = 55;
};

}
