#pragma once

#include "document/document.h"
#include "support/expected.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

[[nodiscard]] Support::Expected<std::vector<uint16_t>, std::string>
RemoveUnusedDefinitions(File& file, std::string_view animation_path);

}
