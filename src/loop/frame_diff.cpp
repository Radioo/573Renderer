#include "loop/frame_diff.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace Loop {

double FrameDiffMad(std::span<const uint8_t> a, std::span<const uint8_t> b) {
    constexpr double kMismatch = 1e9;
    constexpr std::size_t kStep = 16;
    if (a.size() != b.size() || a.empty()) return kMismatch;

    const std::size_t px = a.size() / 4;
    double acc = 0;
    std::size_t cnt = 0;
    for (std::size_t i = 0; i < px; i += kStep) {
        const int d = static_cast<int>(a[(i * 4) + 1]) - static_cast<int>(b[(i * 4) + 1]);
        acc += (d < 0) ? -d : d;
        cnt++;
    }
    return cnt != 0 ? acc / static_cast<double>(cnt) : 0.0;
}

}
