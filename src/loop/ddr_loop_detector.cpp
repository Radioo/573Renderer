#include "loop/ddr_loop_detector.h"

#include "loop/frame_diff.h"

#include <algorithm>

namespace Loop {

namespace {
constexpr double kAfpCleanMad = 2.5;
constexpr int kStaticEnd = 8;
constexpr int kMinLoopFrames = 60;
constexpr double kDivergedMad = 4.0;
constexpr double kDeepFraction = 0.10;
constexpr int kWrapSlack = 4;
}

void DdrLoopDetector::HoldFrame(const DdrFeed& in) {
    held_bgra_.assign(in.bgra.begin(), in.bgra.end());
    held_w_ = in.w;
    held_h_ = in.h;
}

bool DdrLoopDetector::PreRoll(bool afp_ok, int cf, int loop_label_frame) {
    if (!afp_ok || capturing_) return false;
    if (cf >= 0 && cf < loop_label_frame) {
        prev_cf_ = cf;
        return true;
    }
    capturing_ = true;
    cf_start_ = cf;
    wrapped_ = false;
    return false;
}

bool DdrLoopDetector::FirstFrame(const DdrFeed& in, int cf) {
    if (!frame0_bgra_.empty()) return false;
    frame0_bgra_.assign(in.bgra.begin(), in.bgra.end());
    HoldFrame(in);
    loop_held_mad_ = 0.0;
    loop_seen_ = 1;
    prev_cf_ = cf;
    return true;
}

bool DdrLoopDetector::AfpLoop(const DdrFeed& in, int cf, const SubmitFn& submit,
                              DdrResult& result) {
    const int prev_cf = prev_cf_;
    if (prev_cf >= 0 && cf < prev_cf - kWrapSlack) wrapped_ = true;

    if (in.label_active && !wrapped_) {
        cf_static_ = (prev_cf >= 0 && cf == prev_cf) ? cf_static_ + 1 : 0;
        if (cf_static_ >= kStaticEnd) {
            result.finished = submit(held_bgra_.data(), held_w_, held_h_);
            result.reason = DdrReason::LabelEnded;
            result.loops_done = loops_done_;
            result.diag_cf = cf;
            return true;
        }
    }
    prev_cf_ = cf;

    if (!wrapped_ || cf < cf_start_) return false;
    const double wrap_mad = FrameDiffMad(frame0_bgra_, in.bgra);
    if (wrap_mad > kAfpCleanMad && !in.label_active) {
        wrapped_ = false;
        return false;
    }
    afp_loop_ = true;
    if (++loops_done_ < in.loop_count) {
        wrapped_ = false;
        return false;
    }
    result.finished = submit(held_bgra_.data(), held_w_, held_h_);
    result.reason = DdrReason::AfpAuthoredLoop;
    result.loops_done = loops_done_;
    result.diag_cf_start = cf_start_;
    result.diag_mad = wrap_mad;
    return true;
}

bool DdrLoopDetector::ContentLoop(const DdrFeed& in, double mad, const SubmitFn& submit,
                                  DdrResult& result) {
    loop_max_mad_ = std::max(mad, loop_max_mad_);
    if (loop_max_mad_ > kDivergedMad) loop_diverged_ = true;

    const double deep = loop_max_mad_ * kDeepFraction;
    const bool at_loop = loop_diverged_ && loop_seen_ >= kMinLoopFrames && loop_held_mad_ < deep &&
                         mad > loop_held_mad_;
    if (!at_loop) return false;

    if (++loops_done_ >= in.loop_count) {
        result.finished = true;
        result.reason = DdrReason::ContentLoop;
        result.loops_done = loops_done_;
        result.diag_mad = loop_held_mad_;
        return true;
    }
    if (!submit(held_bgra_.data(), held_w_, held_h_)) return true;
    HoldFrame(in);
    loop_held_mad_ = mad;
    loop_max_mad_ = 0.0;
    loop_diverged_ = false;
    loop_seen_ = 1;
    return true;
}

void DdrLoopDetector::CaptureTail(const DdrFeed& in, double mad, const SubmitFn& submit) {
    if (!submit(held_bgra_.data(), held_w_, held_h_)) return;
    HoldFrame(in);
    loop_held_mad_ = mad;
    loop_seen_++;
}

DdrResult DdrLoopDetector::Feed(const DdrFeed& in, const SubmitFn& submit) {
    DdrResult result;
    const bool afp_ok = in.loop_label_frame >= 0;
    const int cf = in.current_frame;

    if (PreRoll(afp_ok, cf, in.loop_label_frame)) return result;
    if (FirstFrame(in, cf)) return result;
    if (afp_ok && cf >= 0 && AfpLoop(in, cf, submit, result)) return result;

    const double mad = FrameDiffMad(frame0_bgra_, in.bgra);
    if (!afp_loop_ && ContentLoop(in, mad, submit, result)) return result;

    CaptureTail(in, mad, submit);
    return result;
}

}
