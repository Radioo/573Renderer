#pragma once

#include <string>

namespace PresetTools {

int DumpDefaults(const std::string& out_dir);

int ExportJson(const std::string& build, const std::string& preset_id, const std::string& out_path);

int Validate(const std::string& path);

}
