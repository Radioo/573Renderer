#pragma once

#include "document/authored.h"
#include "document/filter_fields.h"
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

[[nodiscard]] Support::Expected<void, std::string> SetKeyFilterFieldAt(AuthoredDepth& authored,
                                                                       uint32_t frame,
                                                                       std::string_view field,
                                                                       std::string_view value);

[[nodiscard]] Support::Expected<void, std::string> AddKeyFilterAt(AuthoredDepth& authored,
                                                                  uint32_t frame, NewFilter kind);

[[nodiscard]] Support::Expected<void, std::string>
RemoveKeyFilterAt(AuthoredDepth& authored, uint32_t frame, std::string_view field);

[[nodiscard]] std::optional<Keyframe> KeyAt(const AuthoredDepth& authored,
                                            std::string_view property, uint32_t frame);

[[nodiscard]] std::string KeyValueText(const Keyframe& key);

}
