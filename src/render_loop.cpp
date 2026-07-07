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
#include "loop/cli_autopilot.h"
#include "loop/frame_pacer.h"
#include "loop/submonitor_cycler.h"
#include "window.h"
#include "afp_funcs.h"
#include "avs_boot.h"
#include "afp_boot.h"
#include "game_runtime.h"
#include "game_profile.h"
#include "export.h"
#include "render_backend.h"
#include "mc_control.h"
#include "state/app_state.h"
#include "cli/cli.h"
#include "gui/gui_thread.h"
#include "app_globals.h"
#include "boot.h"
#include "render_loop.h"
#include "render_loop_requests.h"
#include "render/command_list.h"
#include "render_live.h"
#include "render_seh.h"
#include "afp_ddr.h"
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
    in.clip_live = RenderLive::Inspect::HaveActiveClip(AfpManager::StreamId());
    in.active_clip_matches =
        !cli.animation_name.empty() && Runtime::Active().ActiveClipName() == cli.animation_name;
    in.anim_name_matches =
        !cli.animation_name.empty() && AfpManager::AnimName() == cli.animation_name;
    in.modern_stream_valid = AfpManager::StreamId() != 0xFFFFFFFC;
    in.scene_renderable = Runtime::Active().HasRenderableScene(AfpManager::StreamId());
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

std::vector<McControl::ImageSlot> DecodeSubmonitorFrames(const Cli::Options& cli, int& decoded) {
    std::vector<McControl::ImageSlot> slots;
    slots.reserve(cli.submonitor_frames.size());
    decoded = 0;
    for (const auto& f : cli.submonitor_frames) {
        int w = 0;
        int h = 0;
        int const slot = AfpD3D9::LoadExternalImageSlot(f, w, h);
        if (slot < 0) {
            LOG("Main", "submonitor: FAILED to decode frame '%s'", f.c_str());
        } else {
            decoded++;
        }
        slots.push_back({.slot = slot, .w = w, .h = h});
    }
    return slots;
}

void BindSubmonitorFade(const Cli::Options& cli, uint32_t sid,
                        const std::vector<McControl::ImageSlot>& slots, int decoded,
                        std::vector<McControl::ImageSlot>& sm_slots, int& sm_base_mc) {
    int const bound = McControl::BindClipImages(g_afp, sid, cli.submonitor_clip.c_str(),
                                                slots.data(), (int)slots.size());
    sm_base_mc = McControl::FindClip(g_afp, sid, cli.submonitor_clip.c_str());
    if (bound > 0 && sm_base_mc >= 0 && decoded > 0) {
        sm_slots = slots;
        McControl::SetClipVisible(g_afp, sid, "license_usr", false);
        RenderLive::Inspect::GotoLabel(g_afp, cli.submonitor_fade_in_label);
        LOG("Main",
            "submonitor slideshow-fade: %d/%zu frames on "
            "'%s' (mc=%d), dwell=%d fade=%d -> fade cycle",
            decoded, cli.submonitor_frames.size(), cli.submonitor_clip.c_str(), sm_base_mc,
            cli.submonitor_dwell_frames, cli.submonitor_fade_frames);
    } else {
        LOG("Main",
            "submonitor slideshow-fade: clip '%s' not found "
            "or 0 decoded (bound=%d mc=%d) - cannot fade.",
            cli.submonitor_clip.c_str(), bound, sm_base_mc);
    }
}

void BindSubmonitorSlideshow(const Cli::Options& cli, uint32_t sid,
                             const std::vector<McControl::ImageSlot>& slots, int decoded,
                             std::vector<McControl::ImageSlot>& sm_slots, int& sm_base_mc,
                             int& sm_overlay_mc) {
    int ids[8];
    int const ns = McControl::ResolveSiblings(g_afp, sid, cli.submonitor_clip.c_str(), ids, 8);
    if (ns >= 2 && decoded > 0) {
        sm_slots = slots;
        sm_base_mc = cli.submonitor_swap_layers ? ids[1] : ids[0];
        sm_overlay_mc = cli.submonitor_swap_layers ? ids[0] : ids[1];
        McControl::BindImageToMc(g_afp, sm_base_mc, sm_slots[0]);
        McControl::BindImageToMc(g_afp, sm_overlay_mc, sm_slots[1 % (int)sm_slots.size()]);
        LOG("Main",
            "submonitor slideshow: %d/%zu frames, %d "
            "sibling layers (base mc=%d overlay mc=%d), loop=%d "
            "frames -> cross-fade cycle",
            decoded, cli.submonitor_frames.size(), ns, sm_base_mc, sm_overlay_mc,
            cli.submonitor_loop_frames);
    } else {
        LOG("Main",
            "submonitor slideshow: clip '%s' has %d "
            "sibling(s) (<2) or 0 decoded - cannot cross-fade.",
            cli.submonitor_clip.c_str(), ns);
    }
}

void BindSubmonitorStatic(const Cli::Options& cli, uint32_t sid,
                          const std::vector<McControl::ImageSlot>& slots, int decoded) {
    int const bound = McControl::BindClipImages(g_afp, sid, cli.submonitor_clip.c_str(),
                                                slots.data(), (int)slots.size());
    LOG("Main",
        "submonitor: decoded %d/%zu frame(s), bound %d to "
        "clip '%s' on stream 0x%08x",
        decoded, cli.submonitor_frames.size(), bound, cli.submonitor_clip.c_str(), sid);
    if (bound == 0) {
        LOG("Main",
            "submonitor: bound 0 frames - clip '%s' not "
            "found or has no child layers in this animation; the "
            "export will be empty. Check --submonitor-clip / "
            "--animation.",
            cli.submonitor_clip.c_str());
    }
}

void BindSubmonitor(const Cli::Options& cli, std::vector<McControl::ImageSlot>& sm_slots,
                    int& sm_base_mc, int& sm_overlay_mc) {
    const uint32_t sid = AfpManager::StreamId();
    int decoded = 0;
    const std::vector<McControl::ImageSlot> slots = DecodeSubmonitorFrames(cli, decoded);
    if (cli.submonitor_slideshow_fade) {
        BindSubmonitorFade(cli, sid, slots, decoded, sm_slots, sm_base_mc);
    } else if (cli.submonitor_slideshow) {
        BindSubmonitorSlideshow(cli, sid, slots, decoded, sm_slots, sm_base_mc, sm_overlay_mc);
    } else {
        BindSubmonitorStatic(cli, sid, slots, decoded);
    }
}

void PostCliExportRequest(const Cli::Options& cli) {
    LOG("Main", "posting --export request: '%s' fps=%d q=%d", cli.export_path.c_str(),
        cli.export_fps, cli.export_quality);
    App::Request r{};
    r.start_export = true;
    r.export_output_path = cli.export_path;
    r.export_fps = cli.export_fps;
    r.export_quality = cli.export_quality;
    r.export_keyframe_interval = cli.export_keyframe_interval;
    r.export_max_frames = cli.export_max_frames;
    r.export_loop_count = cli.export_loop_count;
    r.export_blend_loop = cli.export_blend_loop;
    r.export_blend_frames = cli.export_blend_frames;
    r.export_bg_transparent = cli.export_bg_transparent;
    r.export_bg_r = cli.export_bg_r;
    r.export_bg_g = cli.export_bg_g;
    r.export_bg_b = cli.export_bg_b;
    r.export_width = cli.export_width;
    r.export_height = cli.export_height;
    r.export_crop_x = cli.export_crop_x;
    r.export_crop_y = cli.export_crop_y;
    r.export_crop_w = cli.export_crop_w;
    r.export_crop_h = cli.export_crop_h;
    r.export_format = cli.export_format;
    r.export_prefer_hardware = cli.export_prefer_hardware;
    r.export_dump_frames_dir = cli.export_dump_frames_dir;
    App::Global().PostRequest(std::move(r));
}

void TickSubmonitorCyclers(const Cli::Options& cli, uint32_t stream_id,
                           const std::vector<McControl::ImageSlot>& sm_slots, int sm_base_mc,
                           int sm_overlay_mc, Loop::DissolveCycler& sm_dissolve,
                           Loop::FadeCycler& sm_fade) {
    if (cli.submonitor_slideshow && sm_base_mc >= 0 && !sm_slots.empty() &&
        cli.submonitor_loop_frames > 0) {
        uint32_t cur = 0;
        uint32_t total = 0;
        if (RenderLive::Inspect::ReadPlayhead(stream_id, &cur, &total, nullptr)) {
            const Loop::DissolveCycler::Tick tick = sm_dissolve.Advance(cur, total);
            if (tick.cycle_changed) {
                int const n = (int)sm_slots.size();
                McControl::BindImageToMc(g_afp, sm_base_mc, sm_slots[tick.cycle % n]);
                McControl::BindImageToMc(g_afp, sm_overlay_mc, sm_slots[(tick.cycle + 1) % n]);
                LOG("Main", "submonitor slideshow: cycle %d -> base frame %d, overlay frame %d",
                    tick.cycle, tick.cycle % n, (tick.cycle + 1) % n);
            }
        }
    }

    if (cli.submonitor_slideshow_fade && sm_base_mc >= 0 && !sm_slots.empty() &&
        cli.submonitor_dwell_frames > 0 && cli.submonitor_fade_frames > 0) {
        const int n = (int)sm_slots.size();
        const Loop::FadeCycler::Tick tick = sm_fade.Advance();
        if (tick.fade_in) {
            McControl::BindImageToMc(g_afp, sm_base_mc, sm_slots[tick.cycle % n]);
            RenderLive::Inspect::GotoLabel(g_afp, cli.submonitor_fade_in_label);
            LOG("Main", "submonitor fade: frame %d (cycle %d) fade-in", tick.cycle % n, tick.cycle);
        } else if (tick.fade_out) {
            RenderLive::Inspect::GotoLabel(g_afp, cli.submonitor_fade_out_label);
            LOG("Main", "submonitor fade: frame %d fade-out", tick.cycle % n);
        }
        McControl::SetClipVisible(g_afp, stream_id, "license_usr", false);
    }
}

uint32_t ApplyLoopHousekeeping(const Cli::Options& cli, uint32_t stream_id, bool exporting) {
    static int loop_cooldown = 0;
    static uint32_t loop_last_sid = 0xFFFFFFFC;
    static int frames_since_switch = 0;
    if (stream_id != loop_last_sid) {
        loop_cooldown = 0;
        loop_last_sid = stream_id;
        frames_since_switch = 0;
    } else {
        frames_since_switch++;
    }
    if (loop_cooldown > 0) loop_cooldown--;

    const auto live_ov = App::Global().GetLiveOverrides();

    if (!exporting && (g_d3d.device != nullptr)) {
        g_d3d.clear_color = (live_ov.bg_color_index >= 0 && live_ov.bg_color_index < 5)
                                ? App::State::kBgPresets[live_ov.bg_color_index]
                                : 0x00000000U;
    }

    int eff_cont = live_ov.continuous_loop_mode;
    if (eff_cont == 0) eff_cont = 1;
    if (cli.submonitor_slideshow_fade) eff_cont = -1;

    static uint32_t flag_dance_done_for = 0xFFFFFFFC;
    static int flag_dance_mode_seen = 0;
    if (stream_id != 0xFFFFFFFC && (g_afp.afp_set_flag_mask != nullptr) &&
        (flag_dance_done_for != stream_id || flag_dance_mode_seen != eff_cont)) {
        if (eff_cont == 1) {
            g_afp.afp_set_flag_mask(stream_id, 0x200, 0x0);
            g_afp.afp_set_flag_mask(stream_id, 0x1, 0x0);
            g_afp.afp_set_flag_mask(stream_id, 0x1000, 0x1000);
            g_afp.afp_set_flag_mask(stream_id, 0x1, 0x1);
            LOG("Live", "applied continuous-loop flag sequence on stream 0x%08x", stream_id);
        } else if (eff_cont == -1) {
            g_afp.afp_set_flag_mask(stream_id, 0x1000, 0x0);
            LOG("Live", "cleared continuous-loop on stream 0x%08x", stream_id);
        }
        flag_dance_done_for = stream_id;
        flag_dance_mode_seen = eff_cont;
    }
    if (eff_cont == 1 && stream_id != 0xFFFFFFFC && (g_afp.afp_set_flag_mask != nullptr)) {
        g_afp.afp_set_flag_mask(stream_id, 0x1, 0x1);
    }

    RenderLive::PublishLiveState(g_afp, stream_id, frames_since_switch, exporting);

    const Runtime::RootRedrive rr = Runtime::Active().MaybeRedriveRootLoop(
        stream_id, loop_cooldown, frames_since_switch, live_ov.trim_frames);
    if (rr.replayed) {
        stream_id = rr.new_stream_id;
        loop_last_sid = stream_id;
        frames_since_switch = 0;
        loop_cooldown = 10;
        if (rr.reset_flag_dance) flag_dance_done_for = 0xFFFFFFFC;
    }
    return stream_id;
}

void ApplyMasterScale(uint32_t stream_id) {
    static float s_last_applied_scale = -1.0F;
    static uint32_t s_last_applied_sid = 0xFFFFFFFC;
    const float scale = App::Global().GetMasterScale();
    const bool stream_changed = (stream_id != s_last_applied_sid);
    const bool scale_changed = (scale != s_last_applied_scale);
    if (stream_id != 0xFFFFFFFC && (g_afp.afp_mc_get != nullptr) &&
        (g_afp.afp_mc_get_id_by_path != nullptr) && (stream_changed || scale_changed)) {
        int const root_mc = g_afp.afp_mc_get_id_by_path(stream_id, "");
        if (root_mc > 0) {
            float xy[2] = {scale, scale};
            constexpr uint32_t kOpSetXyScale = 0x1003;
            constexpr uint32_t kOpInvalidate = 0x101E;
            g_afp.afp_mc_get(root_mc, kOpSetXyScale, (intptr_t)(uintptr_t)xy);
            g_afp.afp_mc_get(root_mc, kOpInvalidate, 1);
        }
        s_last_applied_scale = scale;
        s_last_applied_sid = stream_id;
    }
}

void RenderOneFrame(const Cli::Options& cli, float dt, uint32_t stream_id, int frame_count) {
    if (g_d3d.device != nullptr) {
        g_d3d.BeginFrame();

        Runtime::Active().RenderFrame(dt);

        if (AfpManager::IsBooted() && (g_afp.afp_do_sort_render != nullptr) &&
            stream_id != 0xFFFFFFFC) {
            static bool logged_render_fault = false;
            static bool logged_pre_state = false;
            if (!logged_pre_state) {
                HMODULE afpu_mod = GetModuleHandleA("afp-utils.dll");
                const auto& off = GameProfile::ActiveOffsets();
                if ((afpu_mod != nullptr) && (off.afpu_world_mat_type != 0U) &&
                    (off.afpu_world_mat != 0U)) {
                    uint8_t const mat_type = *((uint8_t*)afpu_mod + off.afpu_world_mat_type);
                    auto* mat = (float*)((uint8_t*)afpu_mod + off.afpu_world_mat);
                    LOG("AFP", "afpu world_mat_type byte = %u, mat[0..7] = %f %f %f %f %f %f %f %f",
                        (unsigned)mat_type, mat[0], mat[1], mat[2], mat[3], mat[4], mat[5], mat[6],
                        mat[7]);
                }
                logged_pre_state = true;
            }
            const RenderSeh::FaultReport sr =
                RenderSeh::SafeCallSortRender(g_afp.afp_do_sort_render);
            if (sr.faulted && !logged_render_fault) {
                RenderSeh::LogFault("afp_do_sort_render", frame_count, sr);
                logged_render_fault = true;
            }
        }

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

void CallAfpUpdateGuarded(float dt, int frame_count) {
    if (!AfpManager::IsBooted() || (g_afp.afp_do_update == nullptr)) return;
    static bool logged_update_fault = false;
    const RenderSeh::FaultReport ur = RenderSeh::SafeCallUpdate(g_afp.afp_do_update, dt);
    if (ur.faulted && !logged_update_fault) {
        LOG("AFP", "afp_do_update threw 0x%08lx at frame %d", (unsigned long)ur.code, frame_count);
        logged_update_fault = true;
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
                             int frame_count, std::vector<McControl::ImageSlot>& sm_slots,
                             int& sm_base_mc, int& sm_overlay_mc) {
    if (act.post_anim_switch) {
        LOG("Main", "posting --animation switch: '%s' (current='%s')", cli.animation_name.c_str(),
            Runtime::Active().ActiveClipName().c_str());
        App::Request r{};
        r.switch_animation = true;
        r.animation_name = cli.animation_name;
        r.animation_label = cli.animation_label;
        App::Global().PostRequest(std::move(r));
    } else if (act.apply_anim_label_inline) {
        LOG("Main", "--animation='%s' already active, no switch needed",
            cli.animation_name.c_str());
        if (!cli.animation_label.empty()) {
            RenderLive::Inspect::GotoLabel(g_afp, cli.animation_label);
            App::Status st = App::Global().GetStatus();
            st.active_label = cli.animation_label;
            st.label_playback_active = true;
            App::Global().SetStatus(st);
        }
    }

    if (act.bind_submonitor) {
        BindSubmonitor(cli, sm_slots, sm_base_mc, sm_overlay_mc);
    }

    if (act.post_seek) {
        LOG("Main", "posting --seek-frame request: %d", cli.seek_frame);
        App::Request r{};
        r.seek_frame = true;
        r.seek_to_frame = cli.seek_frame;
        App::Global().PostRequest(std::move(r));
    }

    if (act.post_goto_label) {
        LOG("Main", "posting --goto-label request: '%s'", cli.goto_label.c_str());
        App::Request r{};
        r.goto_label = true;
        r.goto_label_name = cli.goto_label;
        App::Global().PostRequest(std::move(r));
    }

    if (act.post_export) PostCliExportRequest(cli);

    if (act.post_swap) {
        LOG("Main", "auto-swap: posting hot-swap request for '%s' at frame %d",
            auto_swap_path.c_str(), frame_count);
        App::Request r{};
        r.load_new_ifs = true;
        r.ifs_path = auto_swap_path;
        r.ifs_from_arc = auto_swap_from_arc;
        App::Global().PostRequest(std::move(r));
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

struct SubmonitorBinding {
    std::vector<McControl::ImageSlot> slots;
    int base_mc;
    int overlay_mc;
    Loop::DissolveCycler dissolve;
    Loop::FadeCycler fade;
};

struct FrameTickResult {
    float dt;
    bool exporting;
};

FrameTickResult AdvanceFrame(const Cli::Options& cli, float frame_seconds, int frame_count,
                             SubmonitorBinding& sm) {
    const bool exporting = Export::IsCapturing();
    const int capture_fps = exporting ? Export::TargetFps() : 0;
    float const dt = capture_fps > 0 ? (1.0F / (float)capture_fps) : frame_seconds;
    uint32_t stream_id = AfpManager::StreamId();
    stream_id = Runtime::Active().ActiveClipId(stream_id);

    CallAfpUpdateGuarded(dt, frame_count);

    TickSubmonitorCyclers(cli, stream_id, sm.slots, sm.base_mc, sm.overlay_mc, sm.dissolve,
                          sm.fade);

    if ((frame_count % 120) == 60) Runtime::Active().ReprobeVariantSlots(stream_id);

    stream_id = ApplyLoopHousekeeping(cli, stream_id, exporting);

    ApplyVariants(stream_id);
    ApplySubLayerVisibility(stream_id);
    ApplyMasterScale(stream_id);

    if (g_d3d.device != nullptr) RenderOneFrame(cli, dt, stream_id, frame_count);
    return {.dt = dt, .exporting = exporting};
}

void ShutdownAfterLoop(const Cli::Options& cli, const Render::RenderCommandList& cmd_list,
                       bool have_gui) {
    WriteCmdTrace(cli, cmd_list);

    LOG("Shutdown", "Cleaning up...");
    App::Global().ShouldExit().store(true, std::memory_order_release);
    if (have_gui) GuiThread::Stop();
    Runtime::Active().Shutdown();
    AvsManager::Shutdown(g_avs);
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
    SubmonitorBinding sm{
        .slots = {},
        .base_mc = -1,
        .overlay_mc = -1,
        .dissolve = Loop::DissolveCycler(cli.submonitor_loop_frames),
        .fade = Loop::FadeCycler(cli.submonitor_fade_frames, cli.submonitor_dwell_frames)};

    Render::RenderCommandList cmd_list;
    ArmCommandTaps(cli, cmd_list);

    while (AppWindow::PumpMessages()) {
        const Loop::AutopilotActions act = autopilot.Tick(GatherAutopilotInputs(cli, frame_count));
        if (act.exit_now) {
            LogAutopilotExit(cli, frame_count);
            break;
        }
        ExecuteAutopilotActions(act, cli, auto_swap_path, auto_swap_from_arc, frame_count, sm.slots,
                                sm.base_mc, sm.overlay_mc);

        if (auto req = App::Global().TakeRequest()) {
            DispatchAppRequest(*req);
        }

        const FrameTickResult tick = AdvanceFrame(cli, kFrameSeconds, frame_count, sm);

        frame_count++;

        PublishFpsStats(frame_count, tick.dt, kFrameSeconds, qpc_freq, fps_start, fps_start_frame);

        PaceFrame(pacer, tick.exporting);
    }
    timeEndPeriod(1);

    ShutdownAfterLoop(cli, cmd_list, have_gui);
    return 0;
}
