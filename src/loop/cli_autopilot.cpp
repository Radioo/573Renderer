#include "loop/cli_autopilot.h"

namespace Loop {

CliAutopilot::CliAutopilot(const AutopilotConfig& cfg)
    : cfg_(cfg), submonitor_bound_(!cfg.want_submonitor) {}

AutopilotActions CliAutopilot::Tick(const AutopilotInputs& in) {
    AutopilotActions out;

    if (cfg_.exit_after_frames > 0 && in.frame >= cfg_.exit_after_frames) {
        out.exit_now = true;
        return out;
    }

    if (cfg_.want_animation && !anim_fired_ && in.clip_live) {
        if (in.active_clip_matches) {
            out.apply_anim_label_inline = true;
        } else {
            out.post_anim_switch = true;
        }
        anim_fired_ = true;
    }

    const bool anim_ready = !cfg_.want_animation || in.anim_name_matches;

    if (cfg_.want_submonitor && !submonitor_bound_ && in.modern_stream_valid && anim_ready) {
        out.bind_submonitor = true;
        submonitor_bound_ = true;
    }

    if (cfg_.want_seek && !seek_fired_ && in.clip_live && anim_ready) {
        out.post_seek = true;
        seek_fired_ = true;
    }

    if (cfg_.want_goto_label && !goto_label_fired_ && in.clip_live && anim_ready) {
        out.post_goto_label = true;
        goto_label_fired_ = true;
    }

    if (cfg_.want_export) {
        const bool label_ready = !cfg_.want_goto_label || in.label_applied;
        if (!export_fired_ && in.scene_renderable && anim_ready && label_ready &&
            submonitor_bound_) {
            out.post_export = true;
            export_fired_ = true;
        } else if (export_fired_ && in.export_finished) {
            out.exit_now = true;
            return out;
        }
    }

    if (cfg_.swap_at_frame > 0 && !swap_fired_ && in.frame == cfg_.swap_at_frame) {
        out.post_swap = true;
        swap_fired_ = true;
    }

    return out;
}

}
