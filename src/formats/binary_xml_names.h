#pragma once

#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace BinaryXml::Detail {

[[nodiscard]] Support::Expected<std::string, std::string>
DecodeName(std::span<const uint8_t> bytes, std::size_t& pos, uint8_t signature);

[[nodiscard]] Support::Expected<void, std::string>
EncodeName(const std::string& name, uint8_t signature, std::vector<uint8_t>& out);

}
