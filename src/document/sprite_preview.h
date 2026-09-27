#pragma once

#include "document/clip.h"
#include "document/document.h"
#include "support/expected.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

struct PreviewSymbol {
    std::vector<uint8_t> ifs;
    std::string name;
};

[[nodiscard]] Support::Expected<PreviewSymbol, std::string>
PreviewSymbolFor(const File& file, std::string_view animation_path, ClipId clip);

}
