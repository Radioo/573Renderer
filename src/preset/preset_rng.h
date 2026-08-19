#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Preset {

class CrtRand {
public:
    void Seed(int seed) { state_ = (std::uint32_t)seed; }

    int Next();

private:
    std::uint32_t state_ = 1;
};

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
