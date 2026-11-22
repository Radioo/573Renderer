#pragma once

#include "judge/afp_pose.h"
#include "judge/clip_catalog.h"
#include "judge/judge_backdrop.h"
#include "judge/verdict_store.h"

#include "imgui.h"

#include <cstddef>
#include <optional>
#include <string>

namespace JudgeView {

struct ScanState {
    bool done = false;
    std::size_t read = 0;
    std::size_t total = 0;
    std::string current;
};

struct Model {
    const Judge::Catalog* catalog = nullptr;
    const Judge::VerdictStore* store = nullptr;
    ScanState scan;
    std::size_t current = 0;
    std::string loading;
    std::string clip_error;
    std::string boot_error;
    std::string interp_problem;
    bool paused = false;
    bool loop = true;
    bool next_after_verdict = true;
    std::size_t scale_index = 0;
    AfpPose::MatrixLerp lerp = AfpPose::MatrixLerp::Decompose;
    Judge::Backdrop backdrop = Judge::Backdrop::Black;
    int frame = -1;
    int total = 0;
    float display_hz = 0.0F;
    std::size_t moving = 0;
    std::size_t blended = 0;
    float alpha = 0.0F;
    ImTextureID authored = ImTextureID_Invalid;
    ImTextureID interpolated = ImTextureID_Invalid;
    float stage_w = 720.0F;
    float stage_h = 1280.0F;
};

struct Actions {
    std::optional<std::size_t> select;
    bool toggle_pause = false;
    bool step = false;
    bool restart = false;
    bool toggle_loop = false;
    bool toggle_next_after_verdict = false;
    std::optional<std::size_t> scale_index;
    std::optional<AfpPose::MatrixLerp> lerp;
    std::optional<Judge::Backdrop> backdrop;
    std::optional<Judge::Verdict> verdict;
    bool note_edited = false;
};

[[nodiscard]] ImVec4 VerdictColour(Judge::Verdict verdict);

[[nodiscard]] Actions Build(const Model& model, std::string& note);

}
