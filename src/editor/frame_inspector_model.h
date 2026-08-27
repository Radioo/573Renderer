#pragma once

#include "preset/eval/frame_report.h"

#include <string>
#include <vector>

namespace Editor {

struct FrameRow {
    std::string entity = {};
    std::string label = {};
    std::string value = {};
    std::string clip = {};
    bool header = false;
};

std::vector<FrameRow> FrameRows(const Preset::Eval::FrameReport& report);

}
