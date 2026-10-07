#pragma once

#include "formats/ifs_archive.h"

#include <string>
#include <string_view>

namespace Document {

[[nodiscard]] std::string JoinPath(std::string_view prefix, std::string_view name);

[[nodiscard]] std::string ScriptPath(std::string_view animation_path);

[[nodiscard]] const Ifs::Entry* FindEntry(const Ifs::Archive& archive, std::string_view path);

[[nodiscard]] Ifs::Entry* FindEntry(Ifs::Archive& archive, std::string_view path);

}
