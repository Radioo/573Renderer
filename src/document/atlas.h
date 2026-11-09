#pragma once

#include "support/expected.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Document {

struct AtlasCell {
    std::string name;
    uint32_t width = 0;
    uint32_t height = 0;

    friend bool operator==(const AtlasCell&, const AtlasCell&) = default;
};

struct AtlasPlacement {
    std::string name;
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;

    friend bool operator==(const AtlasPlacement&, const AtlasPlacement&) = default;
};

struct Atlas {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<AtlasPlacement> images;

    friend bool operator==(const Atlas&, const Atlas&) = default;
};

[[nodiscard]] Support::Expected<Atlas, std::string> PackAtlas(std::span<const AtlasCell> cells);

}
