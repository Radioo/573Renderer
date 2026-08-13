#pragma once

#include <string>

namespace PresetTest {

int Run(const std::string& game_dir, const std::string& preset_id, const std::string& out_png,
        int frames, int option);

int RunExport(const std::string& game_dir, const std::string& preset_id,
              const std::string& out_path, int frames, int option);

}
