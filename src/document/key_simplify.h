#pragma once

#include "document/authored.h"
#include "document/key_selection.h"
#include "support/expected.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Document {

[[nodiscard]] Support::Expected<std::vector<KeyRef>, std::string>
SimplifyKeys(AuthoredDepth& authored, const std::vector<KeyRef>& keys, int64_t tolerance);

}
