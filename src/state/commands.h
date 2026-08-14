#pragma once

#include "media/media_format.h"

#include <any>
#include <string>
#include <variant>

namespace App {

struct ExportRequest {
    std::string output_path;
    std::string dump_frames_dir;
    int fps = 60;
    int quality = 60;
    int keyframe_interval = 0;
    int max_frames = 0;
    int loop_count = 1;
    int blend_frames = 15;
    int format = MediaSink::ToIndex(MediaSink::kDefaultFormat);
    int width = 0;
    int height = 0;
    int crop_x = 0;
    int crop_y = 0;
    int crop_w = 0;
    int crop_h = 0;
    float bg_r = 0.13F;
    float bg_g = 0.14F;
    float bg_b = 0.17F;
    bool blend_loop = false;
    bool bg_transparent = true;
    bool prefer_hardware = true;
};

namespace Cmd {

struct BootGame {
    std::string game_dir;
    std::string profile_slug;
    int render_width = 0;
    int render_height = 0;
    bool size_explicit = false;
};

struct LoadContent {
    std::string path;
    bool from_arc = false;
};

struct StartExport {
    ExportRequest req;
};

struct CancelExport {};

struct BackendCommand {
    std::any payload;
};

}

using Command = std::variant<Cmd::BootGame, Cmd::LoadContent, Cmd::StartExport, Cmd::CancelExport,
                             Cmd::BackendCommand>;

}
