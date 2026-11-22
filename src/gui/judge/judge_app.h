#pragma once

#include <string>

namespace JudgeApp {

struct Options {
    std::string game_dir;
    std::string profile;
    std::string verdicts_path;
    std::string start_clip;
    std::string capture_dir;
    int capture_frames = 0;
    int display_hz = 0;
    std::string background;
};

[[nodiscard]] std::string DefaultVerdictsPath(const std::string& profile_slug);

int Run(const Options& options);

}
