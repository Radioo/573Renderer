#include "gui_tl_modals.h"

#include "editor/preset_editor_state.h"
#include "editor/timeline_edits.h"
#include "imgui.h"
#include "preset/doc/preset_validate.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

constexpr const char* kTitle = "Problems";

std::vector<Doc::Problem> g_problems;
unsigned g_revision = 0;
bool g_loaded = false;
bool g_requested = false;

void Refresh() {
    const Editor::State& editor = Editor::Global();
    if (!editor.Loaded()) {
        g_problems.clear();
        g_loaded = false;
        return;
    }
    if (g_loaded && editor.Revision() == g_revision) return;
    g_revision = editor.Revision();
    g_loaded = true;
    g_problems = Doc::Validate(editor.Document());
}

}

const std::vector<Doc::Problem>& CurrentProblems() {
    Refresh();
    return g_problems;
}

int ErrorCount() {
    return (int)std::ranges::count_if(CurrentProblems(), [](const Doc::Problem& problem) {
        return problem.severity == Doc::Severity::Error;
    });
}

void ResetProblems() {
    g_requested = false;
    g_problems.clear();
    g_loaded = false;
}

void RequestProblems() {
    g_requested = true;
}

void RenderProblems() {
    if (g_requested) {
        ImGui::OpenPopup(kTitle);
        g_requested = false;
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
    ImGui::SetNextWindowSizeConstraints(ImVec2(720, 0), ImVec2(720, viewport->WorkSize.y - 48.0F));
    if (!ImGui::BeginPopupModal(kTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

    const std::vector<Doc::Problem>& problems = CurrentProblems();
    ImGui::TextDisabled("%zu problem(s). Clicking a row selects the clip it names.",
                        problems.size());
    ImGui::Separator();
    for (std::size_t i = 0; i < problems.size(); i++) {
        const Doc::Problem& problem = problems[i];
        const bool error = problem.severity == Doc::Severity::Error;
        const std::string text =
            (error ? "error  " : "warning  ") + problem.path + "  " + problem.message;
        const float wrap = ImGui::GetContentRegionAvail().x;
        const float height = ImGui::CalcTextSize(text.c_str(), nullptr, false, wrap).y;
        const ImVec2 origin = ImGui::GetCursorPos();
        const bool clicked =
            ImGui::Selectable(("###tl_problem_" + std::to_string(i)).c_str(), false,
                              ImGuiSelectableFlags_None, ImVec2(0.0F, height));
        ImGui::SetCursorPos(origin);
        ImGui::PushStyleColor(ImGuiCol_Text, error ? ImVec4(1.0F, 0.55F, 0.45F, 1.0F)
                                                   : ImVec4(1.0F, 0.80F, 0.40F, 1.0F));
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap);
        ImGui::TextUnformatted(text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        if (!clicked) continue;
        Editor::State& editor = Editor::Global();
        if (editor.Loaded() && Editor::FindClip(editor.Document(), problem.path).Valid()) {
            editor.Select(problem.path);
        }
    }
    if (problems.empty()) ImGui::TextDisabled("The document validates cleanly.");

    ImGui::Separator();
    const bool escape = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    if (ImGui::Button("Close###tl_problems_close") || escape) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

}
