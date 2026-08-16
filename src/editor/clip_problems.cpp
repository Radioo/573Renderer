#include "editor/clip_problems.h"

#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_validate.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Editor {

namespace Doc = Preset::Doc;

ClipProblems ProblemsForClip(const std::vector<Doc::Problem>& problems, std::string_view clip_id) {
    ClipProblems out;
    if (clip_id.empty()) return out;
    for (const Doc::Problem& problem : problems) {
        if (problem.path != clip_id && problem.related != clip_id) continue;
        if (out.messages.empty() || problem.severity == Doc::Severity::Error) {
            out.severity = problem.severity == Doc::Severity::Error || out.Failing()
                               ? Doc::Severity::Error
                               : problem.severity;
        }
        out.messages.push_back(problem.message);
    }
    return out;
}

std::string GateText(const Doc::Clip& clip) {
    if (!clip.when.has_value()) return "(always)";
    const Doc::Gate& gate = *clip.when;
    std::string joined;
    for (const std::string& choice : gate.choices) {
        if (!joined.empty()) joined += ", ";
        joined += choice;
    }
    if (joined.empty()) joined = "(no choice)";
    const auto kind = (std::size_t)gate.kind;
    return gate.option + " " + std::string(Doc::kGateKindNames[kind]) + " " + joined;
}

}
