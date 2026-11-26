#include "gui/judge/judge_view.h"

#include "gui/gui_dpi.h"
#include "gui/gui_icons.h"
#include "gui/gui_style.h"
#include "gui/gui_widgets.h"
#include "judge/afp_pose.h"
#include "judge/judge_backdrop.h"
#include "judge/verdict_store.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <utility>

namespace JudgeView {

namespace {

constexpr float kListWidthDips = 400.0F;
constexpr float kControlsHeightDips = 288.0F;
constexpr float kNoteHeightDips = 64.0F;
constexpr std::array<const char*, 5> kScaleLabels{"1x", "0.5x", "0.25x", "0.1x", "0.05x"};
constexpr std::array<const char*, 2> kLerpLabels{"Decompose", "Linear"};
constexpr std::array<Judge::Verdict, 3> kVerdicts{Judge::Verdict::Smooth, Judge::Verdict::Wrong,
                                                  Judge::Verdict::Unsure};

const char* VerdictLabel(Judge::Verdict verdict) {
    switch (verdict) {
    case Judge::Verdict::Smooth:
        return "Smooth";
    case Judge::Verdict::Wrong:
        return "Wrong";
    case Judge::Verdict::Unsure:
        return "Unsure";
    }
    return "Unsure";
}

int ResizeString(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag != ImGuiInputTextFlags_CallbackResize) return 0;
    auto* text = static_cast<std::string*>(data->UserData);
    text->resize(static_cast<std::size_t>(data->BufTextLen));
    data->Buf = text->data();
    return 0;
}

void PlaybackKeys(const Model& model, Actions& out) {
    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) out.toggle_pause = true;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_Period))
        out.step = true;
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) out.restart = true;
    if (ImGui::IsKeyPressed(ImGuiKey_L, false)) out.toggle_loop = true;
    if (ImGui::IsKeyPressed(ImGuiKey_M, false)) {
        out.lerp = model.lerp == AfpPose::MatrixLerp::Decompose ? AfpPose::MatrixLerp::Linear
                                                                : AfpPose::MatrixLerp::Decompose;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_B, false)) {
        const auto at = static_cast<std::size_t>(model.backdrop);
        out.backdrop = Judge::kBackdrops.at((at + 1) % Judge::kBackdrops.size());
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket) && model.scale_index + 1 < kScaleLabels.size())
        out.scale_index = model.scale_index + 1;
    if (ImGui::IsKeyPressed(ImGuiKey_RightBracket) && model.scale_index > 0)
        out.scale_index = model.scale_index - 1;
}

void ClipKeys(const Model& model, Actions& out) {
    const std::size_t count = model.catalog != nullptr ? model.catalog->Size() : 0;
    if (count > 0 && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_J)))
        out.select = std::min(model.current + 1, count - 1);
    if (count > 0 && (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_K)))
        out.select = model.current > 0 ? model.current - 1 : 0;
    if (count > 0 && model.store != nullptr && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
        if (auto next = model.catalog->NextUnjudged(model.current, *model.store)) out.select = next;
    }
    const std::array<ImGuiKey, 3> keys{ImGuiKey_1, ImGuiKey_2, ImGuiKey_3};
    for (std::size_t i = 0; i < keys.size(); i++) {
        if (count > 0 && ImGui::IsKeyPressed(keys[i], false)) out.verdict = kVerdicts[i];
    }
}

void Keyboard(const Model& model, Actions& out) {
    if (ImGui::GetIO().WantTextInput) return;
    PlaybackKeys(model, out);
    ClipKeys(model, out);
}

void Progress(const Model& model) {
    if (!model.scan.done) {
        const float done = model.scan.total > 0 ? static_cast<float>(model.scan.read) /
                                                      static_cast<float>(model.scan.total)
                                                : 0.0F;
        char label[64];
        std::snprintf(label, sizeof(label), "%zu / %zu packages", model.scan.read,
                      model.scan.total);
        ImGui::ProgressBar(done, ImVec2(-1.0F, 0.0F), label);
        ImGui::TextWrapped("Reading %s", model.scan.current.c_str());
        return;
    }
    if (model.catalog == nullptr || model.store == nullptr) return;
    const std::size_t judged = model.catalog->JudgedCount(*model.store);
    const std::size_t total = model.catalog->Size();
    char label[64];
    std::snprintf(label, sizeof(label), "%zu of %zu judged", judged, total);
    ImGui::ProgressBar(total > 0 ? static_cast<float>(judged) / static_cast<float>(total) : 0.0F,
                       ImVec2(-1.0F, 0.0F), label);
}

void ClipRow(const Model& model, std::size_t index, Actions& out) {
    const Judge::ClipKey& key = model.catalog->Key(index);
    const Judge::Entry* entry = model.store != nullptr ? model.store->Find(key) : nullptr;
    ImGui::PushID(static_cast<int>(index));
    const std::string label = key.package + "  " + key.clip;
    if (ImGui::Selectable(label.c_str(), index == model.current)) out.select = index;
    if (index == model.current && ImGui::IsWindowAppearing()) ImGui::SetScrollHereY();
    if (entry != nullptr && entry->verdict) {
        ImGui::SameLine(ImGui::GetContentRegionMax().x - Gui::Dpi::S(64.0F));
        ImGui::TextColored(VerdictColour(*entry->verdict), "%s", VerdictLabel(*entry->verdict));
    }
    ImGui::PopID();
}

void ClipList(const Model& model, Actions& out) {
    ImGui::BeginChild("clip_list", ImVec2(Gui::Dpi::S(kListWidthDips), 0.0F),
                      ImGuiChildFlags_Borders);
    Gui::PushHeaderFont();
    ImGui::TextUnformatted("SDVX 1 clips");
    ImGui::PopFont();
    Progress(model);
    ImGui::Separator();
    if (model.catalog != nullptr) {
        ImGui::BeginChild("rows");
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(model.catalog->Size()));
        if (model.current < model.catalog->Size())
            clipper.IncludeItemByIndex(static_cast<int>(model.current));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                ClipRow(model, static_cast<std::size_t>(row), out);
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();
}

void Stage(const Model& model, float height) {
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float gap = Gui::Dpi::S(12.0F);
    const float caption = ImGui::GetTextLineHeightWithSpacing();
    const float scale = std::max(0.05F, std::min((avail_w - gap) / (2.0F * model.stage_w),
                                                 (height - caption) / model.stage_h));
    const ImVec2 size(model.stage_w * scale, model.stage_h * scale);
    const std::array<std::pair<const char*, ImTextureID>, 2> sides{
        {{"Authored (the game)", model.authored},
         {model.lerp == AfpPose::MatrixLerp::Decompose ? "Interpolated (decompose)"
                                                       : "Interpolated (linear)",
          model.interpolated}}};
    for (std::size_t i = 0; i < sides.size(); i++) {
        if (i > 0) ImGui::SameLine(0.0F, gap);
        ImGui::BeginGroup();
        ImGui::TextUnformatted(sides[i].first);
        if (sides[i].second != ImTextureID_Invalid) {
            ImGui::Image(ImTextureRef(sides[i].second), size);
        } else {
            ImGui::Dummy(size);
        }
        ImGui::EndGroup();
    }
}

void Transport(const Model& model, Actions& out) {
    if (ImGui::Button(model.paused ? ICON_PLAY "  Play" : ICON_PAUSE "  Pause"))
        out.toggle_pause = true;
    ImGui::SameLine();
    if (ImGui::Button(ICON_STEP_FWD "  Step")) out.step = true;
    ImGui::SameLine();
    if (ImGui::Button(ICON_JUMP_BACK "  Restart")) out.restart = true;
    ImGui::SameLine();
    bool loop = model.loop;
    if (ImGui::Checkbox("Loop", &loop)) out.toggle_loop = true;
    ImGui::SameLine();
    ImGui::TextUnformatted("Speed");
    ImGui::SameLine();
    int scale = static_cast<int>(model.scale_index);
    if (Gui::Segmented("speed", kScaleLabels.data(), static_cast<int>(kScaleLabels.size()), &scale))
        out.scale_index = static_cast<std::size_t>(scale);
    ImGui::TextUnformatted("Matrix lerp");
    ImGui::SameLine();
    int lerp = model.lerp == AfpPose::MatrixLerp::Decompose ? 0 : 1;
    if (Gui::Segmented("lerp", kLerpLabels.data(), static_cast<int>(kLerpLabels.size()), &lerp))
        out.lerp = lerp == 0 ? AfpPose::MatrixLerp::Decompose : AfpPose::MatrixLerp::Linear;
    ImGui::SameLine();
    ImGui::TextUnformatted("Background");
    ImGui::SameLine();
    std::array<const char*, Judge::kBackdrops.size()> backdrop_labels{};
    for (std::size_t i = 0; i < backdrop_labels.size(); i++)
        backdrop_labels[i] = Judge::BackdropName(Judge::kBackdrops[i]);
    int backdrop = static_cast<int>(model.backdrop);
    if (Gui::Segmented("backdrop", backdrop_labels.data(), static_cast<int>(backdrop_labels.size()),
                       &backdrop))
        out.backdrop = Judge::kBackdrops.at(static_cast<std::size_t>(backdrop));
    Gui::PushMonoFont();
    ImGui::Text("frame %3d / %3d   display %5.1f Hz   moving %zu   blended %zu   alpha %.2f",
                model.frame, model.total, model.display_hz, model.moving, model.blended,
                model.alpha);
    ImGui::PopFont();
}

void Verdicts(const Model& model, std::string& note, Actions& out) {
    const Judge::Entry* entry = nullptr;
    if (model.catalog != nullptr && model.store != nullptr && model.current < model.catalog->Size())
        entry = model.store->Find(model.catalog->Key(model.current));
    for (std::size_t i = 0; i < kVerdicts.size(); i++) {
        if (i > 0) ImGui::SameLine();
        const bool chosen = entry != nullptr && entry->verdict == kVerdicts[i];
        if (chosen) ImGui::PushStyleColor(ImGuiCol_Text, VerdictColour(kVerdicts[i]));
        char label[32];
        std::snprintf(label, sizeof(label), "%s (%zu)", VerdictLabel(kVerdicts[i]), i + 1);
        if (ImGui::Button(label)) out.verdict = kVerdicts[i];
        if (chosen) ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    bool next = model.next_after_verdict;
    if (ImGui::Checkbox("Next unjudged clip after a verdict", &next))
        out.toggle_next_after_verdict = true;
    if (entry != nullptr && entry->verdict) {
        ImGui::SameLine();
        ImGui::TextDisabled("given under %s", AfpPose::MatrixLerpName(entry->matrix_lerp));
    }
    ImGui::TextUnformatted("Note (saved as you type)");
    if (ImGui::InputTextMultiline("##note", note.data(), note.capacity() + 1,
                                  ImVec2(-1.0F, Gui::Dpi::S(kNoteHeightDips)),
                                  ImGuiInputTextFlags_CallbackResize, ResizeString, &note)) {
        out.note_edited = true;
    }
    ImGui::TextDisabled(
        "Space play/pause   Right step   R restart   L loop   [ ] speed   "
        "M lerp mode   B background   Up/Down clip   N next unjudged   1/2/3 verdict");
}

void Problems(const Model& model) {
    for (const std::string* problem :
         {&model.boot_error, &model.interp_problem, &model.clip_error}) {
        if (problem->empty()) continue;
        ImGui::TextColored(VerdictColour(Judge::Verdict::Wrong), "%s", problem->c_str());
    }
    if (!model.loading.empty()) ImGui::Text("Loading %s", model.loading.c_str());
}

}

ImVec4 VerdictColour(Judge::Verdict verdict) {
    switch (verdict) {
    case Judge::Verdict::Smooth:
        return {0.66F, 0.97F, 0.66F, 1.0F};
    case Judge::Verdict::Wrong:
        return {1.0F, 0.82F, 0.80F, 1.0F};
    case Judge::Verdict::Unsure:
        return {1.0F, 0.92F, 0.62F, 1.0F};
    }
    return {1.0F, 1.0F, 1.0F, 1.0F};
}

Actions Build(const Model& model, std::string& note) {
    Actions out;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("##judge", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    Keyboard(model, out);
    ClipList(model, out);
    ImGui::SameLine();
    ImGui::BeginChild("stage", ImVec2(0.0F, 0.0F));
    if (model.catalog != nullptr && model.current < model.catalog->Size()) {
        const Judge::ClipKey& key = model.catalog->Key(model.current);
        Gui::PushHeaderFont();
        ImGui::Text("%s / %s", key.package.c_str(), key.clip.c_str());
        ImGui::PopFont();
    }
    Problems(model);
    const float stage_h = ImGui::GetContentRegionAvail().y - Gui::Dpi::S(kControlsHeightDips);
    Stage(model, std::max(stage_h, Gui::Dpi::S(120.0F)));
    Transport(model, out);
    ImGui::Separator();
    Verdicts(model, note, out);
    ImGui::EndChild();
    ImGui::End();
    return out;
}

}
