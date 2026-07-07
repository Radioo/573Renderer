#include "export_internal.h"
#include "afp_ddr.h"
#include "loop/ddr_loop_detector.h"
#include "support/log.h"

#include <cstdint>
#include <vector>

namespace Export {

void HandleDdrLoopFrame(Session& sess, std::vector<uint8_t>& bgra, int w, int h) {
    if (sess.ddr_loop_label == -2) sess.ddr_loop_label = DdrAfp::ClipLabelFrame("loop");
    const bool afp_ok = (sess.ddr_loop_label >= 0);
    const int cf = afp_ok ? DdrAfp::ClipCurrentFrame() : -1;

    const Loop::DdrFeed in{.bgra = bgra,
                           .w = w,
                           .h = h,
                           .current_frame = cf,
                           .loop_label_frame = sess.ddr_loop_label,
                           .loop_count = sess.loop_count,
                           .label_active = sess.label_active};
    const Loop::DdrResult r = sess.ddr_detector.Feed(in, [&sess](uint8_t* p, int fw, int fh) {
        SubmitOneFrame(sess, p, fw, fh);
        return sess.active;
    });
    sess.loops_done = sess.ddr_detector.LoopsDone();

    if (!r.finished || !sess.active) return;
    sess.loop_detected = true;
    switch (r.reason) {
    case Loop::DdrReason::LabelEnded:
        LOG("Export",
            "DDR label '%s' ended: current_frame static at %d (played once, no loop) - %d frames",
            sess.label_name.c_str(), r.diag_cf, sess.frames_captured);
        break;
    case Loop::DdrReason::AfpAuthoredLoop:
        LOG("Export",
            "DDR afp authored loop: current_frame back to %d (diff=%.2f) - %d loop(s) = %d frames",
            r.diag_cf_start, r.diag_mad, sess.loop_count, sess.frames_captured);
        break;
    case Loop::DdrReason::ContentLoop:
        LOG("Export",
            "DDR loop point found (content): held frame diff=%.2f - %d loop(s) = %d frames",
            r.diag_mad, sess.loop_count, sess.frames_captured);
        break;
    case Loop::DdrReason::None:
        break;
    }
}

}
