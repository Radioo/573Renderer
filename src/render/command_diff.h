#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "render/command_list.h"

namespace Render {

struct CommandDivergence {
    size_t index = 0;
    std::string reason;
};

[[nodiscard]] std::optional<CommandDivergence>
DiffCommandLists(const RenderCommandList& ref, const RenderCommandList& actual, float epsilon);

}
