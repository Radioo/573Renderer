#pragma once

#include <cstdint>
#include <string_view>

namespace Preset {

enum class LayerVerdict : uint8_t {
    Unclassified,
    Background,
    Chrome,
};

LayerVerdict VerdictFor(std::string_view package_dir, std::string_view layer);

std::string_view VerdictName(LayerVerdict verdict);

}
