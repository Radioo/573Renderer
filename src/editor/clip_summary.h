#pragma once

#include "preset/doc/preset_document.h"

#include <string>
#include <string_view>

namespace Editor {

std::string CommandSummary(const Preset::Doc::Command& command, std::string_view target);

std::string ClipSummary(const Preset::Doc::Clip& clip, std::string_view target);

}
