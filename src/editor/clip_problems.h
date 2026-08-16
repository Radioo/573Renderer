#pragma once

#include "preset/doc/preset_document.h"
#include "preset/doc/preset_validate.h"

#include <string>
#include <string_view>
#include <vector>

namespace Editor {

struct ClipProblems {
    Preset::Doc::Severity severity = Preset::Doc::Severity::Warning;
    std::vector<std::string> messages;

    [[nodiscard]] bool Any() const { return !messages.empty(); }
    [[nodiscard]] bool Failing() const { return Any() && severity == Preset::Doc::Severity::Error; }
};

ClipProblems ProblemsForClip(const std::vector<Preset::Doc::Problem>& problems,
                             std::string_view clip_id);

std::string GateText(const Preset::Doc::Clip& clip);

}
