#pragma once

namespace Loop {

struct AutopilotConfig {
    bool want_animation = false;
    bool want_submonitor = false;
    bool want_seek = false;
    bool want_goto_label = false;
    bool want_export = false;
    int swap_at_frame = 0;
    int exit_after_frames = 0;
};

struct AutopilotInputs {
    int frame = 0;
    bool clip_live = false;
    bool active_clip_matches = false;
    bool anim_name_matches = false;
    bool modern_stream_valid = false;
    bool scene_renderable = false;
    bool label_applied = false;
    bool export_finished = false;
};

struct AutopilotActions {
    bool exit_now = false;
    bool post_anim_switch = false;
    bool apply_anim_label_inline = false;
    bool bind_submonitor = false;
    bool post_seek = false;
    bool post_goto_label = false;
    bool post_export = false;
    bool post_swap = false;
};

class CliAutopilot {
public:
    explicit CliAutopilot(const AutopilotConfig& cfg);

    AutopilotActions Tick(const AutopilotInputs& in);

    [[nodiscard]] bool SubmonitorBound() const { return submonitor_bound_; }
    [[nodiscard]] bool ExportFired() const { return export_fired_; }

private:
    AutopilotConfig cfg_;
    bool anim_fired_ = false;
    bool submonitor_bound_ = false;
    bool seek_fired_ = false;
    bool goto_label_fired_ = false;
    bool export_fired_ = false;
    bool swap_fired_ = false;
};

}
