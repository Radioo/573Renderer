#pragma once

#include "formats/afp_animation.h"
#include "support/expected.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace Document {

[[nodiscard]] std::span<const std::string_view> ScriptCalls();

[[nodiscard]] Support::Expected<AfpAnimation::Bytecode, std::string>
CompileScript(AfpAnimation::Animation& animation, std::string_view source);

[[nodiscard]] std::optional<std::string> ScriptSourceText(const AfpAnimation::Animation& animation,
                                                          const AfpAnimation::Bytecode& bytecode);

}
