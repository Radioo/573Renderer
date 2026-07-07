#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace Loop {

using SubmitFn = std::function<bool(uint8_t* bgra, int w, int h)>;

struct DdrFeed {
    std::span<const uint8_t> bgra;
    int w = 0;
    int h = 0;
    int current_frame = -1;
    int loop_label_frame = -1;
    int loop_count = 1;
    bool label_active = false;
};

enum class DdrReason : uint8_t { None, LabelEnded, AfpAuthoredLoop, ContentLoop };

struct DdrResult {
    bool finished = false;
    DdrReason reason = DdrReason::None;
    int loops_done = 0;
    int diag_cf = 0;
    int diag_cf_start = 0;
    double diag_mad = 0.0;
};

class DdrLoopDetector {
public:
    DdrResult Feed(const DdrFeed& in, const SubmitFn& submit);

    [[nodiscard]] int LoopsDone() const { return loops_done_; }

private:
    bool PreRoll(bool afp_ok, int cf, int loop_label_frame);
    bool FirstFrame(const DdrFeed& in, int cf);
    bool AfpLoop(const DdrFeed& in, int cf, const SubmitFn& submit, DdrResult& result);
    bool ContentLoop(const DdrFeed& in, double mad, const SubmitFn& submit, DdrResult& result);
    void CaptureTail(const DdrFeed& in, double mad, const SubmitFn& submit);
    void HoldFrame(const DdrFeed& in);

    std::vector<uint8_t> frame0_bgra_;
    std::vector<uint8_t> held_bgra_;
    int held_w_ = 0;
    int held_h_ = 0;
    double loop_held_mad_ = 0.0;
    int loop_seen_ = 0;
    double loop_max_mad_ = 0.0;
    bool loop_diverged_ = false;
    bool capturing_ = false;
    int cf_start_ = -1;
    int prev_cf_ = -1;
    bool wrapped_ = false;
    int cf_static_ = 0;
    bool afp_loop_ = false;
    int loops_done_ = 0;
};

}
