#include <catch2/catch_test_macros.hpp>

#include "loop/cli_autopilot.h"

namespace {

Loop::AutopilotInputs Loaded() {
    return {.frame = 1,
            .clip_live = true,
            .active_clip_matches = true,
            .anim_name_matches = true,
            .modern_stream_valid = true,
            .scene_renderable = true,
            .label_applied = true,
            .export_finished = false};
}

}

TEST_CASE("exit_after_frames exits and suppresses everything else") {
    Loop::CliAutopilot pilot({.want_export = true, .exit_after_frames = 5});
    Loop::AutopilotInputs in = Loaded();
    in.frame = 5;
    const Loop::AutopilotActions act = pilot.Tick(in);
    CHECK(act.exit_now);
    CHECK_FALSE(act.post_export);
}

TEST_CASE("animation posts a switch when another clip is active, once") {
    Loop::CliAutopilot pilot({.want_animation = true});
    Loop::AutopilotInputs in = Loaded();
    in.active_clip_matches = false;
    CHECK_FALSE(pilot.Tick({.frame = 0}).post_anim_switch);
    CHECK(pilot.Tick(in).post_anim_switch);
    CHECK_FALSE(pilot.Tick(in).post_anim_switch);
}

TEST_CASE("animation already active applies the label inline instead") {
    Loop::CliAutopilot pilot({.want_animation = true});
    const Loop::AutopilotActions act = pilot.Tick(Loaded());
    CHECK(act.apply_anim_label_inline);
    CHECK_FALSE(act.post_anim_switch);
    CHECK_FALSE(pilot.Tick(Loaded()).apply_anim_label_inline);
}

TEST_CASE("export waits for the animation name gate") {
    Loop::CliAutopilot pilot({.want_animation = true, .want_export = true});
    Loop::AutopilotInputs in = Loaded();
    in.anim_name_matches = false;
    CHECK_FALSE(pilot.Tick(in).post_export);
    in.anim_name_matches = true;
    CHECK(pilot.Tick(in).post_export);
}

TEST_CASE("export waits for the goto-label to be applied") {
    Loop::CliAutopilot pilot({.want_goto_label = true, .want_export = true});
    Loop::AutopilotInputs in = Loaded();
    in.label_applied = false;
    const Loop::AutopilotActions first = pilot.Tick(in);
    CHECK(first.post_goto_label);
    CHECK_FALSE(first.post_export);
    in.label_applied = true;
    CHECK(pilot.Tick(in).post_export);
}

TEST_CASE("submonitor bind and export fire in the same tick") {
    Loop::CliAutopilot pilot({.want_submonitor = true, .want_export = true});
    const Loop::AutopilotActions act = pilot.Tick(Loaded());
    CHECK(act.bind_submonitor);
    CHECK(act.post_export);
    CHECK(pilot.SubmonitorBound());
}

TEST_CASE("export finish exits on a later tick") {
    Loop::CliAutopilot pilot({.want_export = true});
    CHECK(pilot.Tick(Loaded()).post_export);
    Loop::AutopilotInputs in = Loaded();
    CHECK_FALSE(pilot.Tick(in).exit_now);
    in.export_finished = true;
    CHECK(pilot.Tick(in).exit_now);
}

TEST_CASE("swap fires only on the exact configured frame") {
    Loop::CliAutopilot pilot({.swap_at_frame = 7});
    Loop::AutopilotInputs in = Loaded();
    in.frame = 6;
    CHECK_FALSE(pilot.Tick(in).post_swap);
    in.frame = 7;
    CHECK(pilot.Tick(in).post_swap);
    CHECK_FALSE(pilot.Tick(in).post_swap);
}

TEST_CASE("seek and goto wait for a live clip") {
    Loop::CliAutopilot pilot({.want_seek = true, .want_goto_label = true});
    Loop::AutopilotInputs in = Loaded();
    in.clip_live = false;
    const Loop::AutopilotActions idle = pilot.Tick(in);
    CHECK_FALSE(idle.post_seek);
    CHECK_FALSE(idle.post_goto_label);
    const Loop::AutopilotActions live = pilot.Tick(Loaded());
    CHECK(live.post_seek);
    CHECK(live.post_goto_label);
}
