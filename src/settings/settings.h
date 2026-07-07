#pragma once

#include <string>

namespace Settings {

struct Config {
    std::string game_dir;
    bool loop_master = false;
    bool root_loop_force = false;
    float master_scale = 1.0F;
    int render_width = 1080;
    int render_height = 1920;
    int render_fps = 120;
    std::string game_profile;
};

[[nodiscard]] std::string DefaultIniPath();

[[nodiscard]] bool SameGameDir(const std::string& a, const std::string& b);

[[nodiscard]] Config LoadFrom(const std::string& ini_path);

[[nodiscard]] bool SaveAtomicTo(const Config& c, const std::string& ini_path);

[[nodiscard]] Config Load();

bool SaveAtomic(const Config& c);

}
