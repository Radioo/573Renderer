#pragma once

#include "document/document.h"
#include "support/expected.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Document {

[[nodiscard]] std::string SpriteExportName(const File& file, std::string_view animation_path,
                                           uint16_t sprite);

[[nodiscard]] Support::Expected<void, std::string> NameSpriteExport(File& file,
                                                                    std::string_view animation_path,
                                                                    uint16_t sprite,
                                                                    std::string_view name);

}
