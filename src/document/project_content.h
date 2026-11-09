#pragma once

#include "document/authored.h"
#include "support/expected.h"

#include <nlohmann/json_fwd.hpp>

#include <string>
#include <vector>

namespace Document {

[[nodiscard]] nlohmann::ordered_json WriteContent(const std::vector<AuthoredDepth>& content);

[[nodiscard]] Support::Expected<std::vector<AuthoredDepth>, std::string>
ReadContent(const nlohmann::ordered_json& value);

}
