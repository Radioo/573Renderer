#include "export.h"
#include "afpu_funcs.h"
#include "app_globals.h"
#include "export_internal.h"
#include "afp_boot.h"
#include "afp_ddr.h"
#include "media/media_format.h"
#include "render_live.h"
#include "media_sink.h"
#include "game_runtime.h"
#include "loop/blend_loop.h"
#include "loop/modern_loop.h"
#include "formats/frame_process.h"
#include "state/telemetry.h"
#include "state/app_state.h"
#include "support/log.h"
#include <utility>
#include <ios>
#include <system_error>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace Export {

namespace {
Session g_owned_session{};
}

Session& ActiveSession() {
    return g_owned_session;
}

namespace {

void Publish(const Session& sess, App::ExportPhase phase, const std::string& err = "") {
    App::ExportState s;
    s.phase = phase;
    s.output_path = sess.output_path;
    s.fps = sess.fps;
    s.quality = sess.quality;
    s.frames_captured = sess.frames_captured;
    s.error = err;
    s.start_time = sess.start_time;
    s.bg_transparent = sess.bg_transparent;
    s.bg_r = sess.bg_r;
    s.bg_g = sess.bg_g;
    s.bg_b = sess.bg_b;
    s.format = sess.format;
    s.using_hardware = sess.using_hw;
    App::Global().SetExport(std::move(s));
}

void ApplyBgClearColor(Session& sess, D3D9State& d3d) {
    sess.saved_clear_color = d3d.clear_color;
    d3d.clear_color = 0x00000000U;
}

void RestoreBgClearColor(const Session& sess) {
    if (sess.d3d_ptr != nullptr) {
        sess.d3d_ptr->clear_color = sess.saved_clear_color;
    }
}

void RestoreContinuousLoop(Session& sess) {
    if (!sess.forced_continuous_loop) return;
    auto lo = App::Global().GetLiveOverrides();
    lo.continuous_loop_mode = sess.saved_continuous_loop;
    App::Global().SetLiveOverrides(lo);
    sess.forced_continuous_loop = false;
}

void DumpBgraReference(const std::vector<uint8_t>& bgra, int w, int h, int frame_idx,
                       const std::string& dir) {
    char name[64];
    snprintf(name, sizeof(name), "frame_%06d.bgra", frame_idx);
    auto path = std::filesystem::path(dir) / name;
    std::ofstream f(path, std::ios::binary);
    if (!f) return;
    const char magic[4] = {'B', 'G', 'R', 'A'};
    f.write(magic, 4);
    auto wu = (uint32_t)w;
    auto hu = (uint32_t)h;
    f.write(reinterpret_cast<const char*>(&wu), 4);
    f.write(reinterpret_cast<const char*>(&hu), 4);
    f.write(reinterpret_cast<const char*>(bgra.data()), (std::streamsize)bgra.size());
}

void RemovePartialOutput(const Session& sess) {
    if (sess.output_path.empty()) return;
    std::error_code ec;
    std::filesystem::remove(sess.output_path, ec);
}

namespace {

void InitSessionFromRequest(Session& sess, const App::Request& req, D3D9State& d3d) {
    sess = {};
    sess.active = true;
    sess.output_path = req.export_output_path;
    sess.fps = req.export_fps > 0 ? req.export_fps : 60;
    sess.quality = req.export_quality >= 0 && req.export_quality <= 100 ? req.export_quality : 60;
    sess.keyframe_interval = req.export_keyframe_interval > 0 ? req.export_keyframe_interval : 0;
    sess.max_frames = req.export_max_frames > 0 ? req.export_max_frames : 0;
    sess.loop_count = req.export_loop_count > 0 ? req.export_loop_count : 1;
    sess.blend_loop = req.export_blend_loop;
    sess.blend_frames = req.export_blend_frames > 0 ? req.export_blend_frames : 15;
    sess.bg_transparent = req.export_bg_transparent;
    sess.bg_r = req.export_bg_r;
    sess.bg_g = req.export_bg_g;
    sess.bg_b = req.export_bg_b;
    sess.out_width = req.export_width;
    sess.out_height = req.export_height;
    sess.crop_x = req.export_crop_x;
    sess.crop_y = req.export_crop_y;
    sess.crop_w = req.export_crop_w;
    sess.crop_h = req.export_crop_h;
    sess.format = req.export_format;
    sess.prefer_hw = req.export_prefer_hardware;
    sess.dump_frames_dir = req.export_dump_frames_dir;
    if (!sess.dump_frames_dir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(sess.dump_frames_dir, ec);
    }
    sess.start_time = std::chrono::steady_clock::now();
    sess.start_cooldown = 0;
    sess.d3d_ptr = &d3d;
    sess.prev_playheads.clear();
    sess.idle_frames = 0;

    sess.loops_done = 0;
    sess.blend_buf.clear();
    sess.blend_w = sess.blend_h = 0;
    sess.loop_detected = false;
    sess.ddr_loop_label = -2;

    sess.label_active = false;
    sess.label_name.clear();
    sess.mc_prev_cur = 0xFFFFFFFF;
    sess.label_seen = 0;
}

void ApplyLabelSuffixToOutput(Session& sess) {
    std::string& p = sess.output_path;
    const size_t dot = p.find_last_of('.');
    const size_t slash = p.find_last_of("/\\");
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        p.insert(dot, "_" + sess.label_name);
    } else {
        p += "_" + sess.label_name;
    }
    LOG("Export", "label '%s' selected -> output: %s", sess.label_name.c_str(), p.c_str());
}

void StartDdrPlayback(Session& sess) {
    if (sess.label_active && !sess.label_name.empty()) {
        DdrAfp::GotoLabel(sess.label_name);
        int const lf = DdrAfp::ClipLabelFrame(sess.label_name.c_str());
        if (lf >= 0) sess.ddr_loop_label = lf;
        LOG("Export",
            "DDR label export: start from '%s' (frame %d), stop after "
            "%d continuous loop(s)",
            sess.label_name.c_str(), lf, sess.loop_count);
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
}

void ForceContinuousLoopOverride(Session& sess, int new_mode) {
    auto lo = App::Global().GetLiveOverrides();
    sess.saved_continuous_loop = lo.continuous_loop_mode;
    sess.forced_continuous_loop = true;
    lo.continuous_loop_mode = new_mode;
    App::Global().SetLiveOverrides(lo);
}

void StartModernPlayback(Session& sess, AfpFuncs& afp) {
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
}

void LogSessionStart(const Session& sess) {
    char bg[32] = {};
    if (!sess.bg_transparent) {
        snprintf(bg, sizeof(bg), "#%02x%02x%02x", (int)(sess.bg_r * 255), (int)(sess.bg_g * 255),
                 (int)(sess.bg_b * 255));
    }
    char maxf[16] = {};
    if (sess.max_frames > 0) snprintf(maxf, sizeof(maxf), "%d", sess.max_frames);
    LOG("Export", "session started: target='%s' fps=%d q=%d bg=%s max_frames=%s",
        sess.output_path.c_str(), sess.fps, sess.quality, sess.bg_transparent ? "transparent" : bg,
        sess.max_frames > 0 ? maxf : "none");
}

}

void StartSession(Session& sess, const App::Request& req, AfpFuncs& afp,
                  [[maybe_unused]] AfpuFuncs& afpu, [[maybe_unused]] DllLoader& afpu_dll,
                  D3D9State& d3d) {
    if (sess.active) {
        LOG("Export", "start: already running, ignoring");
        return;
    }
    InitSessionFromRequest(sess, req, d3d);
    ApplyBgClearColor(sess, d3d);

    {
        App::Status const status = App::Global().GetStatus();
        sess.label_active = status.label_playback_active;
        sess.label_name = status.active_label;
    }
    if (sess.label_active && !sess.label_name.empty()) ApplyLabelSuffixToOutput(sess);

    sess.ddr = Runtime::Active().IsLegacyDdr();
    if (sess.ddr) {
        StartDdrPlayback(sess);
    } else {
        StartModernPlayback(sess, afp);
    }

    Runtime::Active().SetPaused(afp, false);
    LogSessionStart(sess);
    Publish(sess, App::ExportPhase::Capturing);
}

void CancelSession(Session& sess, D3D9State& d3d) {
    if (!sess.active) {
        Publish(sess, App::ExportPhase::Idle);
        return;
    }
    LOG("Export", "session cancelled (%d frames captured, discarded)", sess.frames_captured);
    sess.active = false;
    sess.sink.Cancel();
    if (sess.d3d_ptr == nullptr) sess.d3d_ptr = &d3d;
    RestoreBgClearColor(sess);
    RestoreContinuousLoop(sess);
    RenderLive::ResetPauseDefend();
    sess.d3d_ptr = nullptr;
    RemovePartialOutput(sess);
    Publish(sess, App::ExportPhase::Idle);
}

void FailSession(Session& sess, const std::string& err) {
    sess.active = false;
    sess.sink.Cancel();
    RestoreBgClearColor(sess);
    RestoreContinuousLoop(sess);
    RenderLive::ResetPauseDefend();
    sess.d3d_ptr = nullptr;
    RemovePartialOutput(sess);
    Publish(sess, App::ExportPhase::Failed, err);
    LOG("Export", "failed: %s", err.c_str());
}

}

namespace {

bool OpenSinkForFirstFrame(Session& sess, int enc_w, int enc_h) {
    MediaSink::Params sp;
    sp.output_path = sess.output_path;
    sp.src_width = enc_w;
    sp.src_height = enc_h;
    sp.out_width = sess.out_width;
    sp.out_height = sess.out_height;
    sp.fps = sess.fps;
    sp.quality = sess.quality;
    sp.keyframe_interval = sess.keyframe_interval;
    sp.format = MediaSink::FromIndex(sess.format);
    sp.prefer_hardware = sess.prefer_hw;
    if (!sess.sink.Open(sp)) {
        FailSession(sess, sess.sink.LastError());
        return false;
    }
    sess.using_hw = sess.sink.UsingHardware();
    return true;
}

}

void SubmitOneFrame(Session& sess, uint8_t* bgra, int w, int h) {
    static std::vector<uint8_t> crop_buf;

    if (!sess.bg_transparent) {
        Frame::CompositeOverOpaqueBg({bgra, (size_t)w * h * 4}, sess.bg_r, sess.bg_g, sess.bg_b);
    }

    const uint8_t* encoder_src = bgra;
    int enc_w = w;
    int enc_h = h;
    if (sess.crop_w > 0 && sess.crop_h > 0) {
        const Frame::CropSpec c = Frame::ClampCropToImage(
            {.x = sess.crop_x, .y = sess.crop_y, .w = sess.crop_w, .h = sess.crop_h}, w, h);
        Frame::CopyCropRegion({bgra, (size_t)w * h * 4}, w, c, crop_buf);
        encoder_src = crop_buf.data();
        enc_w = c.w;
        enc_h = c.h;
    }

    if (!sess.dump_frames_dir.empty()) {
        DumpBgraReference(
            std::vector<uint8_t>(encoder_src, encoder_src + ((size_t)enc_w * enc_h * 4)), enc_w,
            enc_h, sess.frames_captured, sess.dump_frames_dir);
    }

    if (sess.blend_loop) {
        constexpr int kBlendFrameCap = 3000;
        if ((int)sess.blend_buf.size() < kBlendFrameCap) {
            sess.blend_w = enc_w;
            sess.blend_h = enc_h;
            sess.blend_buf.emplace_back(encoder_src, encoder_src + ((size_t)enc_w * enc_h * 4));
            sess.frames_captured++;
        }
        return;
    }

    if (sess.frames_captured == 0 && !OpenSinkForFirstFrame(sess, enc_w, enc_h)) return;

    if (!sess.sink.SubmitFrame(encoder_src, sess.frames_captured)) {
        FailSession(sess, sess.sink.LastError());
        return;
    }
    sess.frames_captured++;
}

namespace {

void CaptureFrame(Session& sess, D3D9State& d3d) {
    static std::vector<uint8_t> bgra_buf;
    int w = 0;
    int h = 0;
    if (!d3d.ReadOffscreenBGRA(bgra_buf, w, h)) {
        FailSession(sess, "D3D9 offscreen readback failed");
        return;
    }

    if (sess.ddr && sess.max_frames == 0) {
        HandleDdrLoopFrame(sess, bgra_buf, w, h);
        return;
    }
    SubmitOneFrame(sess, bgra_buf.data(), w, h);
}

bool BlendComposeAndSubmit(Session& sess) {
    auto& buf = sess.blend_buf;
    const int W = sess.blend_w;
    const int H = sess.blend_h;
    const size_t fsz = (size_t)W * H * 4;

    const Loop::BlendPlan plan = Loop::PlanBlendLoop(buf, sess.blend_frames);
    if (plan.loop_length < 2 || fsz == 0) {
        FailSession(sess, "blend-loop: nothing to compose");
        return false;
    }

    LOG("Export",
        "blend-loop: %d frames buffered -> loop length %d (frame-0 diff %.2f), "
        "crossfade %d frames",
        (int)buf.size(), plan.loop_length, plan.best_mad, plan.crossfade);

    MediaSink::Params sp;
    sp.output_path = sess.output_path;
    sp.src_width = W;
    sp.src_height = H;
    sp.out_width = sess.out_width;
    sp.out_height = sess.out_height;
    sp.fps = sess.fps;
    sp.quality = sess.quality;
    sp.keyframe_interval = sess.keyframe_interval;
    sp.format = MediaSink::FromIndex(sess.format);
    sp.prefer_hardware = sess.prefer_hw;
    if (!sess.sink.Open(sp)) {
        FailSession(sess, sess.sink.LastError());
        return false;
    }
    sess.using_hw = sess.sink.UsingHardware();

    std::vector<uint8_t> frame(fsz);
    for (int i = 0; i < plan.loop_length; ++i) {
        Loop::ComposeBlendFrame(buf, plan, i, frame);
        if (!sess.sink.SubmitFrame(frame.data(), i)) {
            FailSession(sess, sess.sink.LastError());
            return false;
        }
    }
    sess.frames_captured = plan.loop_length;
    buf.clear();
    buf.shrink_to_fit();
    return true;
}

void FinishAndEncode(Session& sess) {
    if (sess.frames_captured == 0) {
        FailSession(sess, "no frames were captured - did the animation never advance?");
        return;
    }

    Publish(sess, App::ExportPhase::Encoding);

    if (sess.blend_loop) {
        if (!BlendComposeAndSubmit(sess)) return;
    }

    if (!sess.sink.Finish()) {
        FailSession(sess, sess.sink.LastError());
        return;
    }

    sess.active = false;
    const char* fmt_label = MediaSink::FormatToken(MediaSink::FromIndex(sess.format));
    LOG("Export", "%s written: %s (%d frames, %d fps, q=%d%s)", fmt_label, sess.output_path.c_str(),
        sess.frames_captured, sess.fps, sess.quality, sess.using_hw ? ", HW NVENC" : "");
    RestoreBgClearColor(sess);
    RestoreContinuousLoop(sess);
    RenderLive::ResetPauseDefend();
    sess.d3d_ptr = nullptr;
    Publish(sess, App::ExportPhase::Done);
}

}

namespace {

void TickDdrCapture(Session& sess, D3D9State& d3d) {
    CaptureFrame(sess, d3d);
    if (!sess.active) return;
    if (sess.loop_detected) {
        FinishAndEncode(sess);
        return;
    }
    if (sess.max_frames > 0 && sess.frames_captured >= sess.max_frames) {
        LOG("Export", "max-frames cap reached (%d), finalising encode", sess.frames_captured);
        FinishAndEncode(sess);
        return;
    }
    constexpr int kDdrSafetyCap = 18000;
    if (sess.frames_captured >= kDdrSafetyCap) {
        LOG("Export", "DDR loop not detected within %d frames, finalising", sess.frames_captured);
        FinishAndEncode(sess);
        return;
    }
    if ((sess.frames_captured & 3) == 0) Publish(sess, App::ExportPhase::Capturing);
}

void MaybeDumpTick(const Session& sess, uint32_t mc_c_now, bool wrapped, uint32_t cur_pos,
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

bool HitSafetyCap(const Session& sess) {
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

}

namespace {

void UpdateIdleFrames(Session& sess, const AfpFuncs& afp) {
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

Loop::ModernTick BuildModernTick(const Session& sess, const AfpFuncs& afp) {
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

}

void OnMainLoopTick(EngineSession& es, D3D9State& d3d) {
    const AfpFuncs& afp = es.afp;
    Session& sess = ActiveSession();
    if (!sess.active) return;

    if (sess.start_cooldown > 0) {
        sess.start_cooldown--;
        return;
    }

    if (sess.ddr) {
        TickDdrCapture(sess, d3d);
        return;
    }

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
            CaptureFrame(sess, d3d);
            if (!sess.active) return;
        }
        FinishAndEncode(sess);
        return;
    }

    CaptureFrame(sess, d3d);
    if (!sess.active) return;

    if (HitSafetyCap(sess)) {
        FinishAndEncode(sess);
        return;
    }

    if ((sess.frames_captured & 3) == 0) Publish(sess, App::ExportPhase::Capturing);
}

bool IsCapturing() {
    return ActiveSession().active;
}
int TargetFps() {
    const Session& sess = ActiveSession();
    return sess.fps > 0 ? sess.fps : 60;
}

void HandleStartRequest(const App::Request& req, EngineSession& es, D3D9State& d3d) {
    StartSession(ActiveSession(), req, es.afp, es.afpu, es.afpu_dll, d3d);
}

void HandleCancelRequest(D3D9State& d3d) {
    CancelSession(ActiveSession(), d3d);
}

}
