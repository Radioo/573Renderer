#pragma once

#include "preset/doc/preset_document.h"
#include "support/expected.h"

#include <string>
#include <string_view>

namespace Preset::Doc {

struct ParseError {
    int line = 0;
    int column = 0;
    std::string path;
    std::string message;
};

using Loaded = Support::Expected<Document, ParseError>;

Loaded Load(std::string_view text);

std::string Save(const Document& document);

}
