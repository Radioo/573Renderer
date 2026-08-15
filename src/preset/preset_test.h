#pragma once

#include <array>
#include <string>
#include <vector>

namespace PresetTest {

struct Job {
    std::string game_dir;
    std::string preset_id;
    std::string json_path;
    std::string out_path;
    int frames = 1;
    std::vector<std::string> options;
    bool force = false;
    bool bg_transparent = false;
    std::array<float, 3> bg_rgb = {0.0F, 0.0F, 0.0F};
};

int Run(const Job& job);

int RunExport(const Job& job);

}
