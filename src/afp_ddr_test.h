#pragma once
#include <string>

namespace DdrTest {
int Run(const std::string& modules_dir, const std::string& arc_path, const std::string& out_png,
        int frames);
}
