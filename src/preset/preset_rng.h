#pragma once

#include <array>
#include <cstddef>

namespace Preset {

class Ran3 {
public:
    void Seed(int seed);

    int Next();

    [[nodiscard]] int Draws() const { return draws_; }

private:
    void Refill();

    std::array<int, 56> table_ = {};
    int index_ = 55;
    int draws_ = 0;
};

}
