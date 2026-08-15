#pragma once

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/eval/frame_state.h"

#include <vector>

namespace Preset::Eval {

struct ResolveInput {
    const Doc::Document* document = nullptr;
    const std::vector<int>* choices = nullptr;
    bool tweens = true;
};

FrameState ResolveFrame(const ResolveInput& input, int frame);

int SelectedChoice(const Doc::OptionSpec& option, const std::vector<int>& choices,
                   std::size_t index);

}
