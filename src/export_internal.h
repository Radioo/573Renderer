#pragma once

#include "afp_boot.h"
#include "loop/ddr_loop_detector.h"
#include "media_sink.h"
#include "render_backend.h"
#include "state/app_state.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace Export {

struct Session {
    bool active = false;
    std::string output_path;
    int fps = 60;
    int quality = 60;
    int keyframe_interval = 0;
    int frames_captured = 0;
    int max_frames = 0;
    std::vector<int> prev_playheads;
    int idle_frames = 0;

    int start_cooldown = 0;
    std::chrono::steady_clock::time_point start_time;

    bool bg_transparent = true;
    float bg_r = 0.0f, bg_g = 0.0f, bg_b = 0.0f;
    int out_width = 0;
    int out_height = 0;

    int crop_x = 0;
    int crop_y = 0;
    int crop_w = 0;
    int crop_h = 0;

    int format = 0;
    bool prefer_hw = true;
    bool using_hw = false;

    uint32_t saved_clear_color = 0;
    D3D9State* d3d_ptr = nullptr;

    std::string dump_frames_dir;

    bool ddr = false;
    bool loop_detected = false;

    Loop::DdrLoopDetector ddr_detector;
    int ddr_loop_label = -2;

    int loop_count = 1;
    int loops_done = 0;
    int saved_continuous_loop = 0;
    bool forced_continuous_loop = false;
    bool hold_mode = false;

    bool label_active = false;
    std::string label_name;
    uint32_t mc_prev_cur = 0xFFFFFFFF;
    int label_seen = 0;
    bool blend_loop = false;
    int blend_frames = 15;
    int blend_w = 0, blend_h = 0;
    std::vector<std::vector<uint8_t>> blend_buf;

    MediaSink::Sink sink;
};

Session& ActiveSession();

void SubmitOneFrame(Session& sess, uint8_t* bgra, int w, int h);

void HandleDdrLoopFrame(Session& sess, std::vector<uint8_t>& bgra, int w, int h);

}
