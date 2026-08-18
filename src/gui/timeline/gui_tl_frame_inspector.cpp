#include "gui_timeline_editor.h"

#include "editor/frame_inspector_model.h"
#include "editor/preset_editor_state.h"
#include "editor/timeline_edits.h"
#include "gui/gui_style.h"
#include "imgui.h"
#include "preset/eval/frame_report.h"
#include "state/app_state.h"
#include "state/telemetry.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace Panels::Timeline {

namespace {

struct ColumnWidths {
    float label = 0.0F;
    float value = 0.0F;
    float clip = 0.0F;
};

ColumnWidths MeasureColumns(const std::vector<Editor::FrameRow>& rows) {
    ColumnWidths widths;
    for (const Editor::FrameRow& row : rows) {
        widths.label = std::max(widths.label, ImGui::CalcTextSize(row.label.c_str()).x);
        widths.clip = std::max(widths.clip, ImGui::CalcTextSize(row.clip.c_str()).x);
        if (!row.header) continue;
        widths.value = std::max(widths.value, ImGui::CalcTextSize(row.value.c_str()).x);
    }
    Gui::PushMonoFont();
    for (const Editor::FrameRow& row : rows) {
        if (row.header) continue;
        widths.value = std::max(widths.value, ImGui::CalcTextSize(row.value.c_str()).x);
    }
    ImGui::PopFont();

    const ImGuiStyle& style = ImGui::GetStyle();
    const float cell = style.CellPadding.x * 2.0F;
    widths.label += cell;
    widths.value += cell;
    widths.clip += cell + (style.FramePadding.x * 2.0F);
    return widths;
}

void DrawHeaderRow(const Editor::FrameRow& row) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", row.label.c_str());
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", row.value.c_str());
    ImGui::TableNextColumn();
}

void DrawValueRow(const Editor::FrameRow& row) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", row.label.c_str());
    ImGui::TableNextColumn();
    Gui::PushMonoFont();
    ImGui::TextUnformatted(row.value.c_str());
    ImGui::PopFont();
    ImGui::TableNextColumn();
    if (row.clip.empty()) {
        ImGui::TextDisabled("-");
        return;
    }
    const std::string label = row.clip + "###frame_win_" + row.entity + "_" + row.label;
    if (ImGui::SmallButton(label.c_str())) {
        Editor::State& editor = Editor::Global();
        if (editor.Loaded() && Editor::FindClip(editor.Document(), row.clip).Valid())
            editor.Select(row.clip);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Clip %s wrote %s here.\nClick to select it in the timeline.",
                          row.clip.c_str(), row.label.c_str());
    }
}

}

void RenderFrameTab() {
    App::Global().RequestPresetFrameReport();
    const App::PresetStatus status = App::Global().GetPresetStatus();
    if (status.frame_report == nullptr) {
        ImGui::TextDisabled("no preset document is playing");
        ImGui::TextDisabled("Pick a screen in the preset library to see the frame it resolves.");
        return;
    }

    const Preset::Eval::FrameReport& report = *status.frame_report;
    Gui::PushMonoFont();
    ImGui::Text("frame %d / %d", report.frame, report.length);
    ImGui::SameLine(0.0F, 12.0F);
    ImGui::TextDisabled("%.2f s at %d fps",
                        (double)report.frame / (double)(report.fps > 0 ? report.fps : 60),
                        report.fps);
    ImGui::PopFont();
    ImGui::SameLine(0.0F, 12.0F);
    ImGui::TextDisabled("%s", status.playing ? "playing" : "paused");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("What the evaluator resolved for the frame under the playhead, and "
                          "which clip wrote each value. Read-only: edit the clip that won.");
    }
    ImGui::Separator();

    const std::vector<Editor::FrameRow> rows = Editor::FrameRows(report);
    const ColumnWidths widths = MeasureColumns(rows);

    if (!ImGui::BeginTable("###frame_rows", 3,
                           ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY |
                               ImGuiTableFlags_ScrollX)) {
        return;
    }
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, widths.label);
    ImGui::TableSetupColumn("resolved", ImGuiTableColumnFlags_WidthFixed, widths.value);
    ImGui::TableSetupColumn("from", ImGuiTableColumnFlags_WidthFixed, widths.clip);
    for (const Editor::FrameRow& row : rows) {
        if (row.header) {
            DrawHeaderRow(row);
            continue;
        }
        DrawValueRow(row);
    }
    ImGui::EndTable();
}

}
