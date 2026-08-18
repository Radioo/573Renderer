#pragma once

#include "preset/doc/preset_document.h"
#include "preset/eval/eval_state.h"
#include "preset/eval/frame_state.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Preset::Eval {

enum class ReportKind : uint8_t {
    Scalar,
    Vector,
    Integer,
    Boolean,
    Text,
};

struct ReportValue {
    std::string field = {};
    std::string clip = {};
    std::string text = {};
    std::array<float, 3> vector = {0.0F, 0.0F, 0.0F};
    float scalar = 0.0F;
    int integer = 0;
    ReportKind kind = ReportKind::Scalar;
};

struct ReportEntity {
    std::string name = {};
    std::string kind = {};
    std::vector<ReportValue> values = {};
};

struct FrameReport {
    int frame = 0;
    int length = 0;
    int fps = 60;
    std::vector<ReportEntity> entities = {};
};

FrameReport BuildFrameReport(const Doc::Document& document, const FrameState& state,
                             const EvalState& runtime);

}
