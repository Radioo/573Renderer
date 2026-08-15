#pragma once

#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/eval/frame_state.h"
#include "preset/preset_host.h"

#include <array>
#include <string>
#include <vector>

namespace PresetHost {

struct Override {
    std::string id;
    Preset::Doc::OverrideValue value;
};

std::vector<ParamView> ListParamViews(const Preset::Eval::FrameState& effective,
                                      const Preset::Eval::FrameState& base,
                                      const std::vector<Override>& overrides);

Preset::Doc::Document WithOverrides(const Preset::Doc::Document& base,
                                    const std::vector<Override>& overrides);

bool WriteOverride(std::vector<Override>& overrides, const std::string& id,
                   const std::array<float, 3>& value, int ivalue,
                   const Preset::Eval::FrameState& effective);

void ClearOverride(std::vector<Override>& overrides, const std::string& id);

void ClearOverrideGroup(std::vector<Override>& overrides, const std::string& group,
                        const Preset::Eval::FrameState& effective);

}
