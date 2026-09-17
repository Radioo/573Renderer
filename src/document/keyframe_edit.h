#pragma once

#include "document/authored.h"
#include "document/keyframes.h"
#include "support/expected.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Document {

[[nodiscard]] Support::Expected<void, std::string>
AddKeyAt(AuthoredDepth& authored, std::string_view property, uint32_t frame);

[[nodiscard]] Support::Expected<void, std::string>
RemoveKeyAt(AuthoredDepth& authored, std::string_view property, uint32_t frame);

[[nodiscard]] Support::Expected<void, std::string> SetKeyValueAt(AuthoredDepth& authored,
                                                                 std::string_view property,
                                                                 uint32_t frame,
                                                                 std::string_view value);

[[nodiscard]] std::optional<Keyframe> KeyAt(const AuthoredDepth& authored,
                                            std::string_view property, uint32_t frame);

[[nodiscard]] std::string KeyValueText(const Keyframe& key);

}
