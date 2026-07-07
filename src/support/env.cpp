#include "support/env.h"

#include <cstdlib>
#include <optional>
#include <string>
#include <windows.h>

namespace Support {

std::optional<std::string> EnvVar(const char* name) {
    const DWORD needed = GetEnvironmentVariableA(name, nullptr, 0);
    if (needed == 0) return std::nullopt;
    std::string value(needed, '\0');
    const DWORD written = GetEnvironmentVariableA(name, value.data(), needed);
    value.resize(written);
    return value;
}

bool EnvFlag(const char* name) {
    return EnvVar(name).has_value();
}

std::optional<int> EnvInt(const char* name) {
    std::optional<std::string> value = EnvVar(name);
    if (!value || value->empty()) return std::nullopt;
    return std::atoi(value->c_str());
}

}
