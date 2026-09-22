#include "document/script_problems.h"

#include <rapidfuzz/fuzz.hpp>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

namespace {

constexpr double kCloseEnough = 70.0;

std::string Nearest(std::string_view typed, const std::vector<std::string>& names) {
    std::string best;
    double closest = kCloseEnough;
    for (const std::string& name : names) {
        const double how = rapidfuzz::fuzz::ratio(typed, name);
        if (how <= closest) continue;
        closest = how;
        best = name;
    }
    return best;
}

std::vector<ScriptProblem> ProblemsOnLine(std::string_view line, std::size_t number,
                                          const std::vector<std::string>& names) {
    std::vector<ScriptProblem> out;
    std::size_t at = 0;
    while (at < line.size()) {
        const std::size_t open = line.find('"', at);
        if (open == std::string_view::npos) break;
        const std::size_t close = line.find('"', open + 1);
        if (close == std::string_view::npos) break;
        const std::string_view named = line.substr(open + 1, close - open - 1);
        at = close + 1;
        if (std::ranges::find(names, named) != names.end()) continue;

        const std::string nearest = Nearest(named, names);
        std::string said = "No name in this animation is called \"" + std::string(named) + "\"";
        if (!nearest.empty()) said += ". Did you mean \"" + nearest + "\"?";
        out.push_back(ScriptProblem{
            .said = said, .line = number, .column = open + 2, .length = named.size()});
    }
    return out;
}

}

std::vector<ScriptProblem> ScriptProblems(std::string_view source,
                                          const std::vector<std::string>& names) {
    std::vector<ScriptProblem> out;
    if (names.empty()) return out;
    std::size_t number = 1;
    std::size_t at = 0;
    while (at <= source.size()) {
        std::size_t stop = source.find('\n', at);
        if (stop == std::string_view::npos) stop = source.size();
        const std::vector<ScriptProblem> here =
            ProblemsOnLine(source.substr(at, stop - at), number, names);
        out.insert(out.end(), here.begin(), here.end());
        if (stop == source.size()) break;
        at = stop + 1;
        number++;
    }
    return out;
}

}
