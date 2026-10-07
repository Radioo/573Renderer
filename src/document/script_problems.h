#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

struct ScriptProblem {
    std::string said;
    std::size_t line = 0;
    std::size_t column = 0;
    std::size_t length = 0;

    friend bool operator==(const ScriptProblem&, const ScriptProblem&) = default;
};

[[nodiscard]] std::vector<ScriptProblem> ScriptProblems(std::string_view source,
                                                        const std::vector<std::string>& names);

}
