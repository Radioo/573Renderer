#pragma once

#include "support/expected.h"

#include <string>
#include <string_view>

namespace Ifs {

[[nodiscard]] Support::Expected<std::string, std::string> EscapeName(std::string_view component);

[[nodiscard]] std::string HashedName(std::string_view logical_name);

[[nodiscard]] bool IsSpecialName(std::string_view name);

}
