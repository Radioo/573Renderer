#pragma once

#include <optional>
#include <string>

namespace Support {

std::optional<std::string> EnvVar(const char* name);

bool EnvFlag(const char* name);

std::optional<int> EnvInt(const char* name);

}
