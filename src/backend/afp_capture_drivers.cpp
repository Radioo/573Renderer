#include "backend/afp_capture_drivers.h"

#include "afp_boot.h"
#include "afp_ddr.h"
#include "app_globals.h"
#include "export_internal.h"
#include "game_runtime.h"
#include "loop/ddr_loop_detector.h"
#include "loop/modern_loop.h"
#include "render_backend.h"
#include "render_live.h"
#include "state/app_state.h"
#include "state/live_controls.h"
#include "support/log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace Backend {

namespace {

bool ReadbackAndSubmit(Export::Session& sess, D3D9State& d3d) {
    static std::vector<uint8_t> bgra_buf;
    int w = 0;
    int h = 0;
    if (!d3d.ReadOffscreenBGRA(bgra_buf, w, h)) {
        Export::FailSession(sess, "D3D9 offscreen readback failed");
        return false;
    }
    Export::SubmitOneFrame(sess, bgra_buf.data(), w, h);
    return true;
}

void ForceContinuousLoopOverride(Export::Session& sess, int new_mode) {
    App::Global().MutateLiveOverrides([&sess, new_mode](App::State::LiveOverrides& lo) {
        sess.saved_continuous_loop = lo.continuous_loop_mode;
        lo.continuous_loop_mode = new_mode;
    });
    sess.forced_continuous_loop = true;
}

void RestoreContinuousLoop(Export::Session& sess) {
    if (!sess.forced_continuous_loop) return;
    App::Global().MutateLiveOverrides([&sess](App::State::LiveOverrides& lo) {
        lo.continuous_loop_mode = sess.saved_continuous_loop;
    });
    sess.forced_continuous_loop = false;
}

void EndCaptureCommon(Export::Session& sess) {
    RestoreContinuousLoop(sess);
    RenderLive::ResetPauseDefend();
}

void MaybeDumpTick(const Export::Session& sess, uint32_t mc_c_now, bool wrapped, uint32_t cur_pos,
                   uint32_t total_len) {
    static int s_td = -1;
    if (s_td < 0) {
        char v[8] = {};
        DWORD const n = GetEnvironmentVariableA("EXPORT_TICK_DUMP", v, sizeof(v));
        s_td = (n > 0 && (v[0] != 0) && v[0] != '0') ? 1 : 0;
    }
    if (s_td != 0) {
        LOG("Export",
            "tick: captured=%d mc_cur=%u wrapped=%d cur_pos=%u/%u loops_done=%u hold=%d idle=%d",
            sess.frames_captured, mc_c_now, (int)wrapped, cur_pos, total_len, sess.loops_done,
            (int)sess.hold_mode, sess.idle_frames);
    }
}

bool HitSafetyCap(const Export::Session& sess) {
    if (sess.max_frames > 0 && sess.frames_captured >= sess.max_frames) {
        LOG("Export", "max-frames cap reached (%d), finalising encode", sess.frames_captured);
        return true;
    }

    constexpr int kHoldSafetyCap = 5400;
    if (sess.hold_mode && sess.max_frames == 0 && sess.frames_captured >= kHoldSafetyCap) {
        LOG("Export",
            "root-loop HOLD: reached safety cap (%d frames) without a "
            "master-end / child-cycle signal - the nested child has no public "
            "afp playhead to bound on (terminator STAGED). Finalising; set "
            "--export-max-frames for an exact length.",
            sess.frames_captured);
        return true;
    }

    constexpr int kLabelSafetyCap = 3600;
    if (sess.label_active && sess.max_frames == 0 && sess.label_seen >= kLabelSafetyCap) {
        LOG("Export",
            "label export reached safety cap (%d ticks) without a loop "
            "wrap - the label may run-to-stop; finalising (captured %d frames)",
            sess.label_seen, sess.frames_captured);
        return true;
    }

    constexpr int kIdleThreshold = 90;
    if (!sess.label_active && sess.max_frames == 0 && sess.idle_frames >= kIdleThreshold) {
        LOG("Export",
            "no afp playhead in the composition advanced for %d ticks - "
            "animation is done, finalising encode (captured %d frames total)",
            sess.idle_frames, sess.frames_captured);
        return true;
    }
    return false;
}

void UpdateIdleFrames(Export::Session& sess, const AfpFuncs& afp) {
    if (sess.label_active || sess.max_frames != 0) return;
    std::vector<int> heads;
    uint32_t cur_pos = 0;
    uint32_t total_len = 0;
    if (AfpManager::ReadLayerPosition(afp, &cur_pos, &total_len)) heads.push_back((int)cur_pos);
    uint32_t mc_cur = 0;
    if (AfpManager::ReadMcPlayhead(afp, &mc_cur, nullptr, nullptr)) heads.push_back((int)mc_cur);
    for (const AfpManager::ChildClip& c : AfpManager::EnumerateChildClips(afp)) {
        if (c.have_playhead) heads.push_back(c.cur);
    }
    if (heads.empty()) {
        sess.idle_frames = 0;
        sess.prev_playheads.clear();
        return;
    }
    if (!sess.prev_playheads.empty()) {
        sess.idle_frames = (heads == sess.prev_playheads) ? sess.idle_frames + 1 : 0;
    }
    sess.prev_playheads = std::move(heads);
}

Loop::ModernTick BuildModernTick(const Export::Session& sess, const AfpFuncs& afp) {
    uint32_t cur_pos = 0;
    uint32_t total_len = 0;
    const bool have_pos = AfpManager::ReadLayerPosition(afp, &cur_pos, &total_len);

    uint32_t mc_c_now = 0;
    const bool mc_valid = AfpManager::ReadMcPlayhead(afp, &mc_c_now, nullptr, nullptr);
    bool is_master_complete = false;
    if (!sess.label_active && (!have_pos || total_len <= 0)) {
        is_master_complete = AfpManager::IsMasterComplete(afp);
    }
    return {.have_pos = have_pos,
            .cur_pos = cur_pos,
            .total_len = total_len,
            .mc_valid = mc_valid,
            .mc_cur = mc_c_now,
            .is_master_complete = is_master_complete,
            .idle_frames = sess.idle_frames,
            .label_active = sess.label_active,
            .loop_count = sess.loop_count,
            .hold_mode = sess.hold_mode};
}

void HandleDdrLoopFrame(Export::Session& sess, std::vector<uint8_t>& bgra, int w, int h) {
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
        Export::SubmitOneFrame(sess, p, fw, fh);
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

void AfpModernCaptureDriver::BeginCapture(Export::Session& sess) {
    const AfpFuncs& afp = g_afp;
    if (sess.label_active) {
        {
            auto lo = App::Global().GetLiveOverrides();
            if (lo.continuous_loop_mode != -1) {
                ForceContinuousLoopOverride(sess, -1);
            }
        }
        AfpManager::GotoLabel(afp, sess.label_name);
        LOG("Export",
            "label export: playing label '%s', stop after %d "
            "mc-playhead loop wrap(s) (continuous-loop flag sequence disabled)",
            sess.label_name.c_str(), sess.loop_count);
        Runtime::Active().SetPaused(afp, false);
        return;
    }
    const auto root_mode = App::Global().GetRootLoopMode();
    const int live_cont = App::Global().GetLiveOverrides().continuous_loop_mode;
    const bool force_root = (root_mode == App::State::RootLoopMode::Force) || (live_cont == 1);
    if (sess.blend_loop) {
        AfpManager::ForceReplay(g_engine);
    } else if (force_root) {
        AfpManager::ForceReplay(g_engine);
        ForceContinuousLoopOverride(sess, 1);
        LOG("Export",
            "root-loop FORCE: driving continuous-loop for loop "
            "capture (was %d)",
            sess.saved_continuous_loop);
    } else {
        AfpManager::SeekFrame(afp, 0);
        sess.hold_mode = true;
        ForceContinuousLoopOverride(sess, 1);
        LOG("Export", "root-loop HOLD: rewound to frame 0 (SeekFrame, no "
                      "remount) + continuous-loop flag sequence ON (keeps the master clock "
                      "ticking so nested children free-run) + NO ForceReplay (which "
                      "would snap them); bound by master end / max_frames / safety "
                      "cap - set --export-max-frames for length");
    }
    Runtime::Active().SetPaused(afp, false);
}

void AfpModernCaptureDriver::TickCapture(Export::Session& sess, D3D9State& d3d) {
    const AfpFuncs& afp = g_afp;
    UpdateIdleFrames(sess, afp);
    const Loop::ModernTick tick = BuildModernTick(sess, afp);
    const Loop::ModernDecision dec =
        Loop::StepModernLoop(tick, sess.mc_prev_cur, sess.loops_done, sess.label_seen);
    MaybeDumpTick(sess, tick.mc_cur, dec.wrapped, tick.cur_pos, tick.total_len);

    if (dec.master_oneshot && sess.loop_count > 1 && sess.idle_frames == 8) {
        LOG("Export",
            "master timeline is a one-shot (output frozen at cur %u/%u); "
            "capturing one cycle - loop_count>1 needs a looping bg (try Continuous loop).",
            tick.cur_pos, tick.total_len);
    }
    if (dec.naturally_done && sess.max_frames == 0) {
        if (dec.wrapped && !sess.label_active) {
            if (!ReadbackAndSubmit(sess, d3d)) return;
            if (!sess.active) return;
        }
        Export::FinishAndEncode(sess);
        return;
    }

    if (!ReadbackAndSubmit(sess, d3d)) return;
    if (!sess.active) return;

    if (HitSafetyCap(sess)) {
        Export::FinishAndEncode(sess);
        return;
    }

    Export::PublishCapturing(sess);
}

void AfpModernCaptureDriver::EndCapture(Export::Session& sess) {
    EndCaptureCommon(sess);
}

void AfpDdrCaptureDriver::BeginCapture(Export::Session& sess) {
    if (sess.label_active && !sess.label_name.empty()) {
        DdrAfp::GotoLabel(sess.label_name);
        int const lf = DdrAfp::ClipLabelFrame(sess.label_name.c_str());
        if (lf >= 0) sess.ddr_loop_label = lf;
        LOG("Export",
            "DDR label export: start from '%s' (frame %d), stop after "
            "%d continuous loop(s)",
            sess.label_name.c_str(), lf, sess.loop_count);
        DdrAfp::SetPaused(false);
        return;
    }
    const int loop_f = DdrAfp::ClipLabelFrame("loop");
    if (loop_f >= 0) {
        DdrAfp::SeekFrame(loop_f);
        sess.ddr_loop_label = loop_f;
        LOG("Export",
            "DDR export: rewound to 'loop' label (frame %d); "
            "detecting loop length from authored wrap",
            loop_f);
    } else {
        DdrAfp::SeekFrame(0);
        LOG("Export", "DDR export: no 'loop' label; rewound to frame 0, "
                      "detecting loop length from rendered content");
    }
    DdrAfp::SetPaused(false);
}

void AfpDdrCaptureDriver::TickCapture(Export::Session& sess, D3D9State& d3d) {
    if (sess.max_frames == 0) {
        static std::vector<uint8_t> bgra_buf;
        int w = 0;
        int h = 0;
        if (!d3d.ReadOffscreenBGRA(bgra_buf, w, h)) {
            Export::FailSession(sess, "D3D9 offscreen readback failed");
            return;
        }
        HandleDdrLoopFrame(sess, bgra_buf, w, h);
    } else {
        if (!ReadbackAndSubmit(sess, d3d)) return;
    }
    if (!sess.active) return;
    if (sess.loop_detected) {
        Export::FinishAndEncode(sess);
        return;
    }
    if (sess.max_frames > 0 && sess.frames_captured >= sess.max_frames) {
        LOG("Export", "max-frames cap reached (%d), finalising encode", sess.frames_captured);
        Export::FinishAndEncode(sess);
        return;
    }
    constexpr int kDdrSafetyCap = 18000;
    if (sess.frames_captured >= kDdrSafetyCap) {
        LOG("Export", "DDR loop not detected within %d frames, finalising", sess.frames_captured);
        Export::FinishAndEncode(sess);
        return;
    }
    Export::PublishCapturing(sess);
}

void AfpDdrCaptureDriver::EndCapture(Export::Session& sess) {
    EndCaptureCommon(sess);
}

}
