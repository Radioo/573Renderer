#pragma once

#include "document/outline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <string>
#include <vector>

namespace Document {

struct CallArgument {
    bool is_string = false;
    std::string text;
};

struct LibraryCall {
    std::string object;
    std::string method;
    std::vector<CallArgument> arguments;
};

[[nodiscard]] std::optional<LibraryCall> ReadLibraryCall(const AfpAnimation::Animation& animation,
                                                         const AfpAnimation::Bytecode& bytecode);

[[nodiscard]] Support::Expected<AfpAnimation::Bytecode, std::string>
WriteLibraryCall(AfpAnimation::Animation& animation, const AfpAnimation::Bytecode& original,
                 const LibraryCall& call);

[[nodiscard]] std::vector<std::string> ScriptListing(const AfpAnimation::Animation& animation,
                                                     const AfpAnimation::Bytecode& bytecode);

[[nodiscard]] std::vector<Field> ScriptFields(const AfpAnimation::Animation& animation,
                                              const AfpAnimation::Bytecode& bytecode);

[[nodiscard]] std::string CallArgumentField(std::size_t index);

[[nodiscard]] std::optional<std::size_t> CallArgumentIndex(std::string_view field);

}
