#include "export.h"
#include "backend/backend.h"
#include "export_capture.h"
#include "export_internal.h"
#include "media/media_format.h"
#include "media_sink.h"
#include "formats/frame_process.h"
#include "loop/blend_loop.h"
#include "render_backend.h"
#include "state/telemetry.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "support/log.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <utility>
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

void DriverEndCapture(Session& sess) {
    if (Backend::Active() != nullptr) {
        Backend::Active()->ExportDriver().EndCapture(sess);
    }
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

void InitSessionFromRequest(Session& sess, const App::ExportRequest& req, D3D9State& d3d) {
    sess = {};
    sess.active = true;
    sess.output_path = req.output_path;
    sess.fps = req.fps > 0 ? req.fps : 60;
    sess.quality = req.quality >= 0 && req.quality <= 100 ? req.quality : 60;
    sess.keyframe_interval = req.keyframe_interval > 0 ? req.keyframe_interval : 0;
    sess.max_frames = req.max_frames > 0 ? req.max_frames : 0;
    sess.loop_count = req.loop_count > 0 ? req.loop_count : 1;
    sess.blend_loop = req.blend_loop;
    sess.blend_frames = req.blend_frames > 0 ? req.blend_frames : 15;
    sess.bg_transparent = req.bg_transparent;
    sess.bg_r = req.bg_r;
    sess.bg_g = req.bg_g;
    sess.bg_b = req.bg_b;
    sess.out_width = req.width;
    sess.out_height = req.height;
    sess.crop_x = req.crop_x;
    sess.crop_y = req.crop_y;
    sess.crop_w = req.crop_w;
    sess.crop_h = req.crop_h;
    sess.format = req.format;
    sess.prefer_hw = req.prefer_hardware;
    sess.dump_frames_dir = req.dump_frames_dir;
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

void StartSession(Session& sess, const App::ExportRequest& req, D3D9State& d3d) {
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

    if (Backend::Active() == nullptr) {
        FailSession(sess, "No backend is running, so nothing can be captured.");
        return;
    }
    Backend::Active()->ExportDriver().BeginCapture(sess);
    if (!sess.active) return;

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
    DriverEndCapture(sess);
    sess.d3d_ptr = nullptr;
    RemovePartialOutput(sess);
    Publish(sess, App::ExportPhase::Idle);
}

}

void FailSession(Session& sess, const std::string& err) {
    sess.active = false;
    sess.sink.Cancel();
    RestoreBgClearColor(sess);
    DriverEndCapture(sess);
    sess.d3d_ptr = nullptr;
    RemovePartialOutput(sess);
    Publish(sess, App::ExportPhase::Failed, err);
    LOG("Export", "failed: %s", err.c_str());
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
    DriverEndCapture(sess);
    sess.d3d_ptr = nullptr;
    Publish(sess, App::ExportPhase::Done);
}

void PublishCapturing(Session& sess) {
    if ((sess.frames_captured & 3) == 0) Publish(sess, App::ExportPhase::Capturing);
}

void OnMainLoopTick(D3D9State& d3d) {
    Session& sess = ActiveSession();
    if (!sess.active) return;

    if (sess.start_cooldown > 0) {
        sess.start_cooldown--;
        return;
    }

    Backend::Active()->ExportDriver().TickCapture(sess, d3d);
}

bool IsCapturing() {
    return ActiveSession().active;
}

Capabilities ActiveCapabilities() {
    if (Backend::Active() == nullptr) return {};
    return Backend::Active()->ExportDriver().Caps();
}
int TargetFps() {
    const Session& sess = ActiveSession();
    return sess.fps > 0 ? sess.fps : 60;
}

int PlannedFrames(int max_frames, int preset_frames, int package_frames) {
    if (max_frames > 0) return max_frames;
    if (preset_frames > 0) return preset_frames;
    return (package_frames > 0) ? package_frames : 0;
}

void HandleStartRequest(const App::ExportRequest& req, D3D9State& d3d) {
    StartSession(ActiveSession(), req, d3d);
}

void HandleCancelRequest(D3D9State& d3d) {
    CancelSession(ActiveSession(), d3d);
}

}
