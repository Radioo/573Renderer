#include "preset/preset_rng.h"

#include <cstddef>

namespace Preset {

namespace {

constexpr int kBig = 1000000000;

int Wrap(int value) {
    return (value < 0) ? (value + kBig) : value;
}

}

int CrtRand::Next() {
    state_ = (state_ * 214013U) + 2531011U;
    return (int)((state_ >> 16U) & 0x7FFFU);
}

void Ran3::Seed(int seed) {
    table_[55] = seed;
    int carry = seed;
    int step = 1;
    for (int k = 1; k <= 54; k++) {
        const auto slot = (size_t)((21 * k) % 55);
        table_[slot] = step;
        step = Wrap(carry - step);
        carry = table_[slot];
    }
    Refill();
    Refill();
    Refill();
    index_ = 55;
    draws_ = 0;
}

void Ran3::Refill() {
    for (size_t i = 1; i <= 24; i++)
        table_[i] = Wrap(table_[i] - table_[i + 31]);
    for (size_t i = 25; i <= 55; i++)
        table_[i] = Wrap(table_[i] - table_[i - 24]);
}

int Ran3::Next() {
    draws_++;
    index_++;
    if (index_ > 55) {
        Refill();
        index_ = 1;
    }
    return table_[(size_t)index_];
}

}
