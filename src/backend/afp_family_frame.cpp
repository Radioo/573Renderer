#include "backend/afp_family_backend.h"

#include "afp_boot.h"
#include "app_globals.h"
#include "cli/cli.h"
#include "game_runtime.h"
#include "loop/cli_autopilot.h"
#include "loop/submonitor_cycler.h"
#include "mc_control.h"
#include "render_backend.h"
#include "render_live.h"
#include "render_seh.h"
#include "state/app_state.h"
#include "state/live_controls.h"
#include "support/log.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Backend {

namespace {

void CallAfpUpdateGuarded(float dt, int frame_count) {
    if (!AfpManager::IsBooted() || (g_afp.afp_do_update == nullptr)) return;
    static bool logged_update_fault = false;
    const RenderSeh::FaultReport ur = RenderSeh::SafeCallUpdate(g_afp.afp_do_update, dt);
    if (ur.faulted && !logged_update_fault) {
        LOG("AFP", "afp_do_update threw 0x%08lx at frame %d", (unsigned long)ur.code, frame_count);
        logged_update_fault = true;
    }
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

}

void AfpFamilyBackend::BindSubmonitorFade(uint32_t sid,
                                          const std::vector<McControl::ImageSlot>& slots,
                                          int decoded) {
    const Cli::Options& cli = *cli_;
    int const bound = McControl::BindClipImages(g_afp, sid, cli.submonitor_clip.c_str(),
                                                slots.data(), (int)slots.size());
    sm_base_mc_ = McControl::FindClip(g_afp, sid, cli.submonitor_clip.c_str());
    if (bound > 0 && sm_base_mc_ >= 0 && decoded > 0) {
        sm_slots_ = slots;
        McControl::SetClipVisible(g_afp, sid, "license_usr", false);
        Runtime::Active().GotoLabel(g_afp, cli.submonitor_fade_in_label);
        LOG("Main",
            "submonitor slideshow-fade: %d/%zu frames on "
            "'%s' (mc=%d), dwell=%d fade=%d -> fade cycle",
            decoded, cli.submonitor_frames.size(), cli.submonitor_clip.c_str(), sm_base_mc_,
            cli.submonitor_dwell_frames, cli.submonitor_fade_frames);
    } else {
        LOG("Main",
            "submonitor slideshow-fade: clip '%s' not found "
            "or 0 decoded (bound=%d mc=%d) - cannot fade.",
            cli.submonitor_clip.c_str(), bound, sm_base_mc_);
    }
}

void AfpFamilyBackend::BindSubmonitorSlideshow(uint32_t sid,
                                               const std::vector<McControl::ImageSlot>& slots,
                                               int decoded) {
    const Cli::Options& cli = *cli_;
    int ids[8];
    int const ns = McControl::ResolveSiblings(g_afp, sid, cli.submonitor_clip.c_str(), ids, 8);
    if (ns >= 2 && decoded > 0) {
        sm_slots_ = slots;
        sm_base_mc_ = cli.submonitor_swap_layers ? ids[1] : ids[0];
        sm_overlay_mc_ = cli.submonitor_swap_layers ? ids[0] : ids[1];
        McControl::BindImageToMc(g_afp, sm_base_mc_, sm_slots_[0]);
        McControl::BindImageToMc(g_afp, sm_overlay_mc_, sm_slots_[1 % (int)sm_slots_.size()]);
        LOG("Main",
            "submonitor slideshow: %d/%zu frames, %d "
            "sibling layers (base mc=%d overlay mc=%d), loop=%d "
            "frames -> cross-fade cycle",
            decoded, cli.submonitor_frames.size(), ns, sm_base_mc_, sm_overlay_mc_,
            cli.submonitor_loop_frames);
    } else {
        LOG("Main",
            "submonitor slideshow: clip '%s' has %d "
            "sibling(s) (<2) or 0 decoded - cannot cross-fade.",
            cli.submonitor_clip.c_str(), ns);
    }
}

void AfpFamilyBackend::BindSubmonitor() {
    const Cli::Options& cli = *cli_;
    const uint32_t sid = AfpManager::StreamId();
    int decoded = 0;
    const std::vector<McControl::ImageSlot> slots = DecodeSubmonitorFrames(cli, decoded);
    if (cli.submonitor_slideshow_fade) {
        BindSubmonitorFade(sid, slots, decoded);
    } else if (cli.submonitor_slideshow) {
        BindSubmonitorSlideshow(sid, slots, decoded);
    } else {
        BindSubmonitorStatic(cli, sid, slots, decoded);
    }
}

void AfpFamilyBackend::TickSubmonitorCyclers(uint32_t stream_id) {
    const Cli::Options& cli = *cli_;
    if (cli.submonitor_slideshow && sm_base_mc_ >= 0 && !sm_slots_.empty() &&
        cli.submonitor_loop_frames > 0) {
        uint32_t cur = 0;
        uint32_t total = 0;
        if (Runtime::Active().ReadPlayhead(stream_id, &cur, &total, nullptr)) {
            const Loop::DissolveCycler::Tick tick = sm_dissolve_.Advance(cur, total);
            if (tick.cycle_changed) {
                int const n = (int)sm_slots_.size();
                McControl::BindImageToMc(g_afp, sm_base_mc_, sm_slots_[tick.cycle % n]);
                McControl::BindImageToMc(g_afp, sm_overlay_mc_, sm_slots_[(tick.cycle + 1) % n]);
                LOG("Main", "submonitor slideshow: cycle %d -> base frame %d, overlay frame %d",
                    tick.cycle, tick.cycle % n, (tick.cycle + 1) % n);
            }
        }
    }

    if (cli.submonitor_slideshow_fade && sm_base_mc_ >= 0 && !sm_slots_.empty() &&
        cli.submonitor_dwell_frames > 0 && cli.submonitor_fade_frames > 0) {
        const int n = (int)sm_slots_.size();
        const Loop::FadeCycler::Tick tick = sm_fade_.Advance();
        if (tick.fade_in) {
            McControl::BindImageToMc(g_afp, sm_base_mc_, sm_slots_[tick.cycle % n]);
            Runtime::Active().GotoLabel(g_afp, cli.submonitor_fade_in_label);
            LOG("Main", "submonitor fade: frame %d (cycle %d) fade-in", tick.cycle % n, tick.cycle);
        } else if (tick.fade_out) {
            Runtime::Active().GotoLabel(g_afp, cli.submonitor_fade_out_label);
            LOG("Main", "submonitor fade: frame %d fade-out", tick.cycle % n);
        }
        McControl::SetClipVisible(g_afp, stream_id, "license_usr", false);
    }
}

uint32_t AfpFamilyBackend::ApplyLoopHousekeeping(uint32_t stream_id, bool exporting) {
    if (stream_id != loop_last_sid_) {
        loop_cooldown_ = 0;
        loop_last_sid_ = stream_id;
        frames_since_switch_ = 0;
    } else {
        frames_since_switch_++;
    }
    if (loop_cooldown_ > 0) loop_cooldown_--;

    const auto live_ov = App::Global().GetLiveOverrides();

    if (!exporting && (g_d3d.device != nullptr)) {
        g_d3d.clear_color = (live_ov.bg_color_index >= 0 && live_ov.bg_color_index < 5)
                                ? App::State::kBgPresets[live_ov.bg_color_index]
                                : 0x00000000U;
    }

    int eff_cont = live_ov.continuous_loop_mode;
    if (eff_cont == 0) eff_cont = 1;
    if (cli_->submonitor_slideshow_fade) eff_cont = -1;

    Runtime::Active().ApplyContinuousLoop(stream_id, eff_cont);

    RenderLive::PublishLiveState(g_afp, stream_id, frames_since_switch_, exporting);

    const Runtime::RootRedrive rr = Runtime::Active().MaybeRedriveRootLoop(
        stream_id, loop_cooldown_, frames_since_switch_, live_ov.trim_frames);
    if (rr.replayed) {
        stream_id = rr.new_stream_id;
        loop_last_sid_ = stream_id;
        frames_since_switch_ = 0;
        loop_cooldown_ = 10;
    }
    return stream_id;
}

void AfpFamilyBackend::AdvanceFrame(float dt, int frame_count, bool exporting) {
    uint32_t stream_id = AfpManager::StreamId();
    stream_id = Runtime::Active().ActiveClipId(stream_id);

    CallAfpUpdateGuarded(dt, frame_count);

    TickSubmonitorCyclers(stream_id);

    if ((frame_count % 120) == 60) Runtime::Active().ReprobeVariantSlots(stream_id);

    stream_id = ApplyLoopHousekeeping(stream_id, exporting);

    Runtime::Active().ApplyVariantSlots(stream_id);
    Runtime::Active().ApplySublayerOverrides(stream_id);
    Runtime::Active().ApplyMasterScale(stream_id);
}

void AfpFamilyBackend::RenderScene(float dt, int frame_count) {
    const uint32_t stream_id = Runtime::Active().ActiveClipId(AfpManager::StreamId());
    Runtime::Active().RenderFrame(dt, stream_id, frame_count);
}

void AfpFamilyBackend::FillAutopilotInputs(Loop::AutopilotInputs& in) {
    const Cli::Options& cli = *cli_;
    in.clip_live = Runtime::Active().HaveActiveClip(AfpManager::StreamId());
    in.active_clip_matches =
        !cli.animation_name.empty() && Runtime::Active().ActiveClipName() == cli.animation_name;
    in.anim_name_matches =
        !cli.animation_name.empty() && AfpManager::AnimName() == cli.animation_name;
    in.modern_stream_valid = AfpManager::StreamId() != Runtime::kModernNoStream;
    in.scene_renderable = Runtime::Active().HasRenderableScene(AfpManager::StreamId());
}

}
