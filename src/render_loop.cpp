#include <string>
#include <cstddef>
#include "gpu_context.h"
#include "state/telemetry.h"
#include <utility>
#include <cstdint>
#include <cstdio>
#include <ios>
#include <atomic>
#include "support/log.h"
#include "backend/backend.h"
#include "loop/cli_autopilot.h"
#include "loop/frame_pacer.h"
#include "window.h"
#include "game_runtime.h"
#include "export.h"
#include "render_backend.h"
#include "backend/afp_commands.h"
#include "state/app_state.h"
#include "state/commands.h"
#include "cli/cli.h"
#include "gui/gui_thread.h"
#include "app_globals.h"
#include "render_loop.h"
#include "render_loop_requests.h"
#include "render/command_list.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shell32.lib")

namespace {

std::string NormalizeSlashes(std::string s) {
    for (auto& c : s)
        if (c == '\\') c = '/';
    return s;
}

void FindIfsBySwapName(App::State& state, const std::string& swap_ifs, std::string& auto_swap_path,
                       bool& auto_swap_from_arc) {
    state.WaitForIfsScan();
    auto list = state.ListAvailableIfs();
    const std::string needle = NormalizeSlashes(swap_ifs);
    for (auto& e : list) {
        const std::string nm = NormalizeSlashes(e.name);
        size_t const slash = nm.find_last_of('/');
        std::string const tail = slash == std::string::npos ? nm : nm.substr(slash + 1);
        if (nm == needle || nm == needle + ".ifs" || tail == needle || tail == needle + ".ifs") {
            auto_swap_path = e.full_path;
            auto_swap_from_arc = e.from_arc;
            break;
        }
    }
}

std::string ResolveAutoSwapPath(const Cli::Options& cli, App::State& state,
                                bool& auto_swap_from_arc) {
    std::string auto_swap_path = cli.swap_ifs;
    auto_swap_from_arc = false;
    if (!std::filesystem::exists(auto_swap_path)) {
        FindIfsBySwapName(state, cli.swap_ifs, auto_swap_path, auto_swap_from_arc);
    }
    if (!auto_swap_from_arc && auto_swap_path.size() > 4 && auto_swap_path.ends_with(".arc")) {
        auto_swap_from_arc = true;
    }
    return auto_swap_path;
}

void LogAutopilotPlans(const Cli::Options& cli) {
    if (!cli.export_path.empty()) {
        LOG("Main", "--export='%s' fps=%d q=%d -> will start export after IFS load",
            cli.export_path.c_str(), cli.export_fps, cli.export_quality);
    }
    if (!cli.animation_name.empty()) {
        LOG("Main", "--animation='%s' -> will switch animation after IFS load",
            cli.animation_name.c_str());
    }
    if (cli.seek_frame >= 0) {
        LOG("Main", "--seek-frame=%d -> will seek after IFS load", cli.seek_frame);
    }
    if (!cli.goto_label.empty()) {
        LOG("Main", "--goto-label='%s' -> will goto after IFS load", cli.goto_label.c_str());
    }
    if (!cli.submonitor_frames.empty()) {
        LOG("Main",
            "--submonitor-frames: %zu frame(s) -> will bind to '%s' "
            "after animation is live",
            cli.submonitor_frames.size(), cli.submonitor_clip.c_str());
    }
}

Loop::AutopilotInputs GatherAutopilotInputs(const Cli::Options& cli, int frame_count) {
    Loop::AutopilotInputs in;
    in.frame = frame_count;
    Backend::Active()->FillAutopilotInputs(in);
    if (!cli.goto_label.empty()) {
        App::Status const st = App::Global().GetStatus();
        in.label_applied = st.label_playback_active && st.active_label == cli.goto_label;
    }
    if (!cli.export_path.empty()) {
        auto ph = App::Global().GetExport().phase;
        in.export_finished = ph == App::ExportPhase::Done || ph == App::ExportPhase::Failed;
    }
    return in;
}

void PostCliExportRequest(const Cli::Options& cli) {
    LOG("Main", "posting --export request: '%s' fps=%d q=%d", cli.export_path.c_str(),
        cli.export_fps, cli.export_quality);
    App::ExportRequest r;
    r.output_path = cli.export_path;
    r.fps = cli.export_fps;
    r.quality = cli.export_quality;
    r.keyframe_interval = cli.export_keyframe_interval;
    r.max_frames = cli.export_max_frames;
    r.loop_count = cli.export_loop_count;
    r.blend_loop = cli.export_blend_loop;
    r.blend_frames = cli.export_blend_frames;
    r.bg_transparent = cli.export_bg_transparent;
    r.bg_r = cli.export_bg_r;
    r.bg_g = cli.export_bg_g;
    r.bg_b = cli.export_bg_b;
    r.width = cli.export_width;
    r.height = cli.export_height;
    r.crop_x = cli.export_crop_x;
    r.crop_y = cli.export_crop_y;
    r.crop_w = cli.export_crop_w;
    r.crop_h = cli.export_crop_h;
    r.format = cli.export_format;
    r.prefer_hardware = cli.export_prefer_hardware;
    r.dump_frames_dir = cli.export_dump_frames_dir;
    App::Global().PostCommand(App::Cmd::StartExport{.req = std::move(r)});
}

void RenderOneFrame(const Cli::Options& cli, float dt, int frame_count) {
    if (g_d3d.device != nullptr) {
        g_d3d.BeginFrame();

        Backend::Active()->RenderScene(dt, frame_count);

        Export::OnMainLoopTick(g_engine, g_d3d);

        int const current_frame = frame_count + 1;
        for (int const sf : cli.screenshot_frames) {
            if (sf == current_frame) {
                char path[512];
                snprintf(path, sizeof(path), "%s%d.png", cli.screenshot_prefix.c_str(), sf);
                D3D9State_RequestScreenshot(path);
                break;
            }
        }

        g_d3d.EndFrame();
    }
}

void PublishFpsStats(int frame_count, float dt, float expected_dt, const LARGE_INTEGER& qpc_freq,
                     LARGE_INTEGER& fps_start, int& fps_start_frame) {
    if ((frame_count % 30) == 0) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double const elapsed =
            double(now.QuadPart - fps_start.QuadPart) / double(qpc_freq.QuadPart);
        double const fps = elapsed > 0 ? (frame_count - fps_start_frame) / elapsed : 0;
        App::Status st = App::Global().GetStatus();
        st.fps_measured = fps;
        st.frame_count = frame_count;
        App::Global().SetStatus(st);
        static int fps_log_cnt = 0;
        if (fps_log_cnt++ < 20) {
            LOG("Main", "fps=%.1f frame=%d (dt=%.6fs expected=%.6fs)", fps, frame_count, (double)dt,
                (double)expected_dt);
        }
        if (elapsed > 2.0) {
            fps_start = now;
            fps_start_frame = frame_count;
        }
    }
}

void WriteCmdTrace(const Cli::Options& cli, const Render::RenderCommandList& cmd_list) {
    if (g_gpu.cmd_list == nullptr) return;
    g_gpu.cmd_list = nullptr;
    const std::string text = Render::FormatCommandList(cmd_list);
    std::ofstream f(cli.cmd_trace_path, std::ios::binary);
    if (f) {
        f.write(text.data(), (std::streamsize)text.size());
        LOG("Main", "--cmd-trace: %zu commands -> %zu bytes to '%s'", cmd_list.size(), text.size(),
            cli.cmd_trace_path.c_str());
    } else {
        LOG("Main", "--cmd-trace: FAILED to open '%s'", cli.cmd_trace_path.c_str());
    }
}

int64_t QpcNow() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

void LogAutopilotExit(const Cli::Options& cli, int frame_count) {
    if (cli.exit_after_frames > 0 && frame_count >= cli.exit_after_frames) {
        LOG("Main", "exit_after_frames=%d reached; exiting.", cli.exit_after_frames);
    } else {
        LOG("Main", "--export finished with phase=%d, exiting.",
            (int)App::Global().GetExport().phase);
    }
}

void PaceFrame(Loop::FramePacer& pacer, bool exporting) {
    if (exporting) {
        pacer.AnchorTo(QpcNow());
    } else {
        pacer.ScheduleNext();
        for (;;) {
            const Loop::FramePacer::Plan plan = pacer.PlanWait(QpcNow());
            if (plan.action == Loop::FramePacer::Wait::Proceed) break;
            if (plan.action == Loop::FramePacer::Wait::Sleep) {
                if (plan.sleep_ms != 0U) Sleep(plan.sleep_ms);
            } else {
                YieldProcessor();
            }
        }
    }
    pacer.ResyncIfBehind(QpcNow());
}

void ExecuteAutopilotActions(const Loop::AutopilotActions& act, const Cli::Options& cli,
                             const std::string& auto_swap_path, bool auto_swap_from_arc,
                             int frame_count) {
    if (act.post_anim_switch) {
        LOG("Main", "posting --animation switch: '%s' (current='%s')", cli.animation_name.c_str(),
            Runtime::Active().ActiveClipName().c_str());
        App::Global().PostCommand(AfpCmd::Wrap(
            AfpCmd::SwitchAnimation{.name = cli.animation_name, .label = cli.animation_label}));
    } else if (act.apply_anim_label_inline) {
        LOG("Main", "--animation='%s' already active, no switch needed",
            cli.animation_name.c_str());
        if (!cli.animation_label.empty()) {
            Runtime::Active().GotoLabel(g_afp, cli.animation_label);
            App::Status st = App::Global().GetStatus();
            st.active_label = cli.animation_label;
            st.label_playback_active = true;
            App::Global().SetStatus(st);
        }
    }

    if (act.bind_submonitor) {
        Backend::Active()->BindSubmonitor();
    }

    if (act.post_seek) {
        LOG("Main", "posting --seek-frame request: %d", cli.seek_frame);
        App::Global().PostCommand(AfpCmd::Wrap(AfpCmd::SeekFrame{.frame = cli.seek_frame}));
    }

    if (act.post_goto_label) {
        LOG("Main", "posting --goto-label request: '%s'", cli.goto_label.c_str());
        App::Global().PostCommand(AfpCmd::Wrap(AfpCmd::GotoLabel{.name = cli.goto_label}));
    }

    if (act.post_export) PostCliExportRequest(cli);

    if (act.post_swap) {
        LOG("Main", "auto-swap: posting hot-swap request for '%s' at frame %d",
            auto_swap_path.c_str(), frame_count);
        App::Global().PostCommand(
            App::Cmd::LoadContent{.path = auto_swap_path, .from_arc = auto_swap_from_arc});
    }
}

int ResolveRenderFps(App::State& state) {
    int render_fps = state.GetRenderFps();
    if (render_fps < 1) render_fps = 120;
    return std::min(render_fps, 1000);
}

std::string ResolveAutoSwapPlan(const Cli::Options& cli, App::State& state,
                                bool& auto_swap_from_arc) {
    std::string auto_swap_path;
    if (cli.swap_after_frames > 0 && !cli.swap_ifs.empty()) {
        auto_swap_path = ResolveAutoSwapPath(cli, state, auto_swap_from_arc);
        LOG("Main", "--swap-after-frames=%d --ifs2='%s' -> will hot-swap at frame %d (from_arc=%d)",
            cli.swap_after_frames, auto_swap_path.c_str(), cli.swap_after_frames,
            (int)auto_swap_from_arc);
    }
    return auto_swap_path;
}

Loop::CliAutopilot MakeAutopilot(const Cli::Options& cli, const std::string& auto_swap_path) {
    return Loop::CliAutopilot({.want_animation = !cli.animation_name.empty(),
                               .want_submonitor = !cli.submonitor_frames.empty(),
                               .want_seek = cli.seek_frame >= 0,
                               .want_goto_label = !cli.goto_label.empty(),
                               .want_export = !cli.export_path.empty(),
                               .swap_at_frame = auto_swap_path.empty() ? 0 : cli.swap_after_frames,
                               .exit_after_frames = cli.exit_after_frames});
}

struct FrameTickResult {
    float dt;
    bool exporting;
};

FrameTickResult AdvanceFrame(const Cli::Options& cli, float frame_seconds, int frame_count) {
    const bool exporting = Export::IsCapturing();
    const int capture_fps = exporting ? Export::TargetFps() : 0;
    float const dt = capture_fps > 0 ? (1.0F / (float)capture_fps) : frame_seconds;

    Backend::Active()->AdvanceFrame(dt, frame_count, exporting);

    if (g_d3d.device != nullptr) RenderOneFrame(cli, dt, frame_count);
    return {.dt = dt, .exporting = exporting};
}

void ShutdownAfterLoop(const Cli::Options& cli, const Render::RenderCommandList& cmd_list,
                       bool have_gui) {
    WriteCmdTrace(cli, cmd_list);

    LOG("Shutdown", "Cleaning up...");
    App::Global().ShouldExit().store(true, std::memory_order_release);
    if (have_gui) GuiThread::Stop();
    Backend::Active()->Shutdown();
    g_d3d.Shutdown();
    Log::Shutdown();
}

void ArmCommandTaps(const Cli::Options& cli, Render::RenderCommandList& cmd_list) {
    if (!cli.cmd_trace_path.empty()) g_gpu.cmd_list = &cmd_list;
    g_gpu.deferred_replay = cli.deferred_replay;
    if (cli.deferred_replay) LOG("Main", "--deferred-replay: whole-frame command replay active");
}

}

int RunRenderLoop(const Cli::Options& cli, bool have_gui) {
    auto& state = App::Global();
    timeBeginPeriod(1);
    LARGE_INTEGER qpc_freq;
    QueryPerformanceFrequency(&qpc_freq);
    const int render_fps = ResolveRenderFps(state);
    Loop::FramePacer pacer(render_fps, qpc_freq.QuadPart);
    pacer.AnchorTo(QpcNow());

    int frame_count = 0;
    const float kFrameSeconds = 1.0F / (float)render_fps;
    LOG("Main", "Render loop frame rate = %d fps (dt=%.6fs)", render_fps, kFrameSeconds);
    LARGE_INTEGER fps_start;
    QueryPerformanceCounter(&fps_start);
    int fps_start_frame = 0;

    LOG("Main", "Entering main loop (ESC to exit)...");
    bool auto_swap_from_arc = false;
    const std::string auto_swap_path = ResolveAutoSwapPlan(cli, state, auto_swap_from_arc);
    Loop::CliAutopilot autopilot = MakeAutopilot(cli, auto_swap_path);
    LogAutopilotPlans(cli);

    Render::RenderCommandList cmd_list;
    ArmCommandTaps(cli, cmd_list);

    while (AppWindow::PumpMessages()) {
        const Loop::AutopilotActions act = autopilot.Tick(GatherAutopilotInputs(cli, frame_count));
        if (act.exit_now) {
            LogAutopilotExit(cli, frame_count);
            break;
        }
        ExecuteAutopilotActions(act, cli, auto_swap_path, auto_swap_from_arc, frame_count);

        if (auto cmd = App::Global().TakeCommand()) {
            DispatchAppCommand(*cmd);
        }

        const FrameTickResult tick = AdvanceFrame(cli, kFrameSeconds, frame_count);

        frame_count++;

        PublishFpsStats(frame_count, tick.dt, kFrameSeconds, qpc_freq, fps_start, fps_start_frame);

        PaceFrame(pacer, tick.exporting);
    }
    timeEndPeriod(1);

    ShutdownAfterLoop(cli, cmd_list, have_gui);
    return 0;
}
