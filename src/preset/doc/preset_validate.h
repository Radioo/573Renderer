#pragma once

#include "preset/doc/preset_document.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Preset::Doc {

enum class Severity : uint8_t {
    Error,
    Warning,
};

struct Problem {
    Severity severity = Severity::Error;
    std::string path;
    std::string related;
    std::string message;
};

std::vector<Problem> Validate(const Document& document,
                              std::span<const std::string_view> builtin_ids = {});

}
