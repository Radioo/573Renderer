#include "loop/blend_loop.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Loop {

namespace {

constexpr std::size_t kByteStride = 16;
constexpr int kMinLoopFloor = 30;
constexpr double kNoMatch = 1e30;

double SparseMad(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size() || a.empty()) return kNoMatch;
    uint64_t acc = 0;
    std::size_t n = 0;
    for (std::size_t p = 0; p < a.size(); p += kByteStride) {
        const int d = static_cast<int>(a[p]) - static_cast<int>(b[p]);
        acc += static_cast<uint64_t>(d < 0 ? -d : d);
        n++;
    }
    return n != 0 ? static_cast<double>(acc) / static_cast<double>(n) : kNoMatch;
}

}

BlendPlan PlanBlendLoop(FrameBuffer& buf, int blend_frames) {
    while (buf.size() >= 2 && buf.back() == buf[buf.size() - 2])
        buf.pop_back();

    BlendPlan plan;
    const int n = static_cast<int>(buf.size());
    if (n < 2) return plan;

    int crossfade = std::max(0, blend_frames);
    const int min_len = std::min(n - 1, std::max(kMinLoopFloor, crossfade + 1));

    int best_len = -1;
    double best_mad = kNoMatch;
    for (int len = min_len; len <= n - 1 && len + crossfade <= n; ++len) {
        const double m = SparseMad(buf[static_cast<std::size_t>(len)], buf[0]);
        if (m < best_mad) {
            best_mad = m;
            best_len = len;
        }
    }
    if (best_len < 0) best_len = n;
    if (best_len + crossfade > n) crossfade = std::max(0, n - best_len);

    plan.loop_length = best_len;
    plan.crossfade = crossfade;
    plan.best_mad = best_mad;
    return plan;
}

void ComposeBlendFrame(const FrameBuffer& buf, const BlendPlan& plan, int index,
                       std::span<uint8_t> out) {
    if (index >= plan.crossfade) {
        const std::vector<uint8_t>& body = buf[static_cast<std::size_t>(index)];
        std::copy_n(body.begin(), std::min(out.size(), body.size()), out.begin());
        return;
    }
    const double x = (index + 0.5) / static_cast<double>(plan.crossfade);
    const double t = x * x * (3.0 - (2.0 * x));
    const std::size_t tail_index =
        static_cast<std::size_t>(plan.loop_length) + static_cast<std::size_t>(index);
    const std::vector<uint8_t>& tail = buf[tail_index];
    const std::vector<uint8_t>& start = buf[static_cast<std::size_t>(index)];
    const std::size_t count = std::min({out.size(), tail.size(), start.size()});
    for (std::size_t p = 0; p < count; ++p) {
        out[p] = static_cast<uint8_t>(std::lround((tail[p] * (1.0 - t)) + (start[p] * t)));
    }
}

}
