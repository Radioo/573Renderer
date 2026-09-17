#pragma once

#include "formats/ifs_archive.h"
#include "support/expected.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Document {

[[nodiscard]] Support::Expected<std::string, std::string> AddAnimation(Ifs::Archive& archive,
                                                                       std::string_view name,
                                                                       std::string_view like_path,
                                                                       uint32_t frames);

[[nodiscard]] Support::Expected<void, std::string> RemoveAnimation(Ifs::Archive& archive,
                                                                   std::string_view path);

}
