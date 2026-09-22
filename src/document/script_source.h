#pragma once

#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace Document {

[[nodiscard]] std::optional<uint16_t> ScriptCallId(std::string_view name);

[[nodiscard]] std::vector<std::string> ScriptCallsUsed(std::string_view source);

[[nodiscard]] bool ScriptIsCalls(std::string_view source);

[[nodiscard]] std::span<const std::string_view> ScriptCalls();

[[nodiscard]] std::span<const std::string_view> ScriptInstructions();

[[nodiscard]] std::span<const std::string_view> ScriptWords();

[[nodiscard]] Support::Expected<AfpAnimation::Bytecode, std::string>
CompileScript(AfpAnimation::Animation& animation, std::string_view source);

[[nodiscard]] std::optional<std::string> ScriptSourceText(const AfpAnimation::Animation& animation,
                                                          const AfpAnimation::Bytecode& bytecode);

}
