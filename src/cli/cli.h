#pragma once

#include <string>
#include <vector>

namespace Cli {

struct Options {
    struct SlotOverride {
        std::string path;
        bool visible = true;
        std::string bitmap;
    };

    struct SubLayerOverride {
        std::string path;
        bool visible = true;
    };

    std::vector<int> screenshot_frames;
    std::vector<SlotOverride> slot_overrides;
    std::vector<SubLayerOverride> sublayer_overrides;
    std::vector<std::string> submonitor_frames;
    std::string startup_ifs;
    std::string animation_name;
    std::string animation_label;
    std::string goto_label;
    std::string game_dir;
    std::string game_profile;
    std::string extract_qpro_dir;
    std::string qpro_parts;
    std::string qpro_only;
    std::string qpro_dump_ifs;
    std::string qpro_body_one;
    std::string qpro_back_one;
    std::string qpro_head_one;
    std::string qpro_hand_one;
    std::string qpro_hair_one;
    std::string qpro_face_one;
    std::string qpro_clip_one;
    std::string qpro_hand_composite;
    std::string qpro_head_composite;
    std::string qpro_back_composite;
    std::string swap_ifs;
    std::string screenshot_prefix = "screenshots/auto_f";
    std::string export_path;
    std::string export_dump_frames_dir;
    std::string cmd_trace_path;
    std::string submonitor_clip = "subbg_usr/bg_usr";
    std::string submonitor_fade_in_label = "fade_in";
    std::string submonitor_fade_out_label = "fade_out";
    float master_scale = 0.0F;
    float afp_speed = 0.0F;
    int continuous_loop_mode = 0;
    int root_loop_mode = -1;
    int seek_frame = -1;
    int mc_name_type = 0;
    int render_width = 0;
    int render_height = 0;
    int render_fps = 0;
    int qpro_fps = 0;
    int swap_after_frames = 0;
    int exit_after_frames = 0;
    int export_fps = 60;
    int export_quality = 60;
    int export_keyframe_interval = 0;
    int export_max_frames = 0;
    int export_loop_count = 1;
    int export_blend_frames = 15;
    int export_width = 0;
    int export_height = 0;
    int export_crop_x = 0;
    int export_crop_y = 0;
    int export_crop_w = 0;
    int export_crop_h = 0;
    float export_bg_r = 0.0F;
    float export_bg_g = 0.0F;
    float export_bg_b = 0.0F;
    int export_format = 0;
    int submonitor_loop_frames = 0;
    int submonitor_dwell_frames = 720;
    int submonitor_fade_frames = 120;
    bool headless = false;
    bool no_gui = false;
    bool boot_ifses = false;
    bool start_paused = false;
    bool filter_enabled = false;
    bool show_mc_names = false;
    bool qpro_no_hue_scope = false;
    bool deferred_replay = false;
    bool export_blend_loop = false;
    bool export_bg_transparent = true;
    bool export_prefer_hardware = true;
    bool submonitor_slideshow = false;
    bool submonitor_swap_layers = false;
    bool submonitor_slideshow_fade = false;
};

bool Parse(int argc, char** argv, Options& out, std::string& err);

void PrintUsage();

}
