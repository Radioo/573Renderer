#pragma once

#include "document/authored.h"
#include "document/key_selection.h"
#include "support/expected.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Document {

struct Wiggle {
    uint32_t every = 1;
    int64_t magnitude = 0;
    uint32_t seed = 0;
};

[[nodiscard]] Support::Expected<std::vector<KeyRef>, std::string>
WiggleKeys(AuthoredDepth& authored, const std::vector<KeyRef>& keys, const Wiggle& wiggle);

}
