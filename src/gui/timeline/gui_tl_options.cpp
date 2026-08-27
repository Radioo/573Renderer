#include "gui_tl_internal.h"

#include "editor/options_model.h"
#include "editor/preset_editor_state.h"
#include "gui/gui_dpi.h"
#include "imgui.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "state/app_state.h"
#include "state/preset_commands.h"
#include "state/telemetry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

constexpr float kOptionRowHDips = 30.0F;
constexpr float kChipWDips = 4.0F;
constexpr float kEditWDips = 56.0F;

float OptionRowH() {
    return Gui::Dpi::S(kOptionRowHDips);
}

float ChipW() {
    return Gui::Dpi::S(kChipWDips);
}

float EditW() {
    return Gui::Dpi::S(kEditWDips);
}

int SelectedChoice(const Ctx& ctx, std::size_t option) {
    const std::vector<int>& choices = ctx.status.option_choices;
    const Doc::OptionSpec& spec = ctx.document->options[option];
    if (spec.choices.empty()) return -1;
    const int wanted = (option < choices.size()) ? choices[option] : spec.default_choice;
    return std::clamp(wanted, 0, (int)spec.choices.size() - 1);
}

void PostOption(int option, int choice) {
    App::Global().PostCommand(
        PresetCmd::Wrap(PresetCmd::SetOption{.option = option, .choice = choice}));
}

void DrawHeader(Ctx& ctx, const Doc::OptionSpec& spec, std::size_t index, float y) {
    const float bottom = y + OptionRowH();
    ctx.draw->AddRectFilled(ImVec2(ctx.header_x, y),
                            ImVec2(ctx.header_x + ChipW(), bottom - Gui::Dpi::S(2.0F)),
                            CommandColor(Doc::CommandType::OptionSelect));

    ImGui::SetCursorScreenPos(ImVec2(ctx.header_x, y));
    ImGui::InvisibleButton(
        ("###tl_opt_head_" + spec.id).c_str(),
        ImVec2(std::max(1.0F, ctx.lane_x - ctx.header_x - EditW() - Gui::Dpi::S(8.0F)),
               OptionRowH() - Gui::Dpi::S(2.0F)));
    const std::string label = spec.label.empty() ? spec.id : spec.label;
    ctx.draw->AddText(ImVec2(ctx.header_x + Gui::Dpi::S(10.0F), y + Gui::Dpi::S(2.0F)),
                      ImGui::GetColorU32(ImGuiCol_TextDisabled), "OPT");
    ctx.draw->AddText(
        ImVec2(ctx.header_x + Gui::Dpi::S(10.0F), y + Gui::Dpi::S(15.0F)),
        ImGui::GetColorU32(ImGuiCol_Text),
        Ellipsized(label, ctx.lane_x - ctx.header_x - EditW() - Gui::Dpi::S(20.0F)).c_str());

    ImGui::SetCursorScreenPos(
        ImVec2(ctx.lane_x - EditW() - Gui::Dpi::S(4.0F), y + Gui::Dpi::S(3.0F)));
    if (ImGui::Button(("Edit...###tl_opt_edit_" + spec.id).c_str(), ImVec2(EditW(), 0.0F))) {
        ctx.editor->PostRequest(
            Editor::Request{.kind = Editor::RequestKind::OptionProperties, .index = (int)index});
    }
}

void DrawChoices(Ctx& ctx, const Doc::OptionSpec& spec, std::size_t index, int selected, float y) {
    ImGui::SetCursorScreenPos(ImVec2(ctx.lane_x + Gui::Dpi::S(4.0F), y + Gui::Dpi::S(3.0F)));
    for (std::size_t i = 0; i < spec.choices.size(); i++) {
        const bool on = std::cmp_equal(i, selected);
        if (on) {
            const ImU32 accent = ImGui::GetColorU32(ImGuiCol_CheckMark);
            ImGui::PushStyleColor(ImGuiCol_Button, accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_WindowBg));
        }
        const std::string id = "###tl_opt_choice_" + spec.id + "_" + std::to_string(i);
        if (ImGui::Button((spec.choices[i].label + id).c_str())) PostOption((int)index, (int)i);
        if (on) ImGui::PopStyleColor(4);
        ImGui::SameLine(0.0F, Gui::Dpi::S(2.0F));
    }
    const std::string tail =
        (selected >= 0 ? "selected " + spec.choices[(std::size_t)selected].label + "   " : "") +
        Editor::TransitionSummary(spec.transition);
    ImGui::TextDisabled("%s", tail.c_str());
}

void DashedRect(const Ctx& ctx, float x0, float y0, float x1, float y1, ImU32 color) {
    const auto steps = (int)std::floor((x1 - x0) / DashPitch());
    for (int i = 0; i <= steps; i++) {
        const float from = x0 + ((float)i * DashPitch());
        const float to = std::min(from + DashLength(), x1);
        ctx.draw->AddLine(ImVec2(from, y0), ImVec2(to, y0), color, Gui::Dpi::S(1.0F));
        ctx.draw->AddLine(ImVec2(from, y1), ImVec2(to, y1), color, Gui::Dpi::S(1.0F));
    }
    DashedVertical(ctx, x0, y0, y1, color);
    DashedVertical(ctx, x1, y0, y1, color);
}

const Doc::OptionSpec* TransitioningOption(const Ctx& ctx) {
    const App::PresetTransition& live = ctx.status.transition;
    if (live.option < 0 || live.frames_left <= 0) return nullptr;
    if (std::cmp_greater_equal(live.option, ctx.document->options.size())) return nullptr;
    return &ctx.document->options[(std::size_t)live.option];
}

}

float OptionsBandHeight(const Doc::Document& document) {
    if (document.options.empty()) return 0.0F;
    return (float)document.options.size() * OptionRowH();
}

void DrawOptionsBand(Ctx& ctx, float y) {
    for (std::size_t i = 0; i < ctx.document->options.size(); i++) {
        const Doc::OptionSpec& spec = ctx.document->options[i];
        const float row = y + ((float)i * OptionRowH());
        ctx.draw->AddRectFilled(
            ImVec2(ctx.header_x, row),
            ImVec2(ctx.lane_x + ctx.lane_w, row + OptionRowH() - Gui::Dpi::S(2.0F)),
            ImGui::GetColorU32(ImGuiCol_FrameBg));
        DrawHeader(ctx, spec, i, row);
        DrawChoices(ctx, spec, i, SelectedChoice(ctx, i), row);
    }
}

void DrawOptionsOverlay(const Ctx& ctx) {
    const Doc::OptionSpec* spec = TransitioningOption(ctx);
    if (spec == nullptr) return;
    const App::PresetTransition& live = ctx.status.transition;
    const int span = Editor::TransitionSpanFrames(
        Doc::Transition{.frames = live.frames_left, .step = spec->transition.step});
    if (span <= 0) return;

    const float x0 = std::max(FrameToX(ctx, ctx.status.frame), ctx.lane_x);
    const float x1 = std::min(FrameToX(ctx, ctx.status.frame + span), ctx.lane_x + ctx.lane_w);
    const std::vector<std::string> rows =
        Editor::TrackIdsForTargets(*ctx.document, Editor::MovedTargets(*spec, live.from, live.to));
    const ImU32 accent = ImGui::GetColorU32(ImGuiCol_CheckMark);
    if (std::ranges::find(rows, Editor::kOptionBandRow) != rows.end()) {
        const float row = ctx.options_top + ((float)live.option * OptionRowH());
        DashedRect(ctx, x0, row + Gui::Dpi::S(1.0F), x1, row + OptionRowH() - Gui::Dpi::S(3.0F),
                   accent);
    }
    for (const TrackBand& band : ctx.bands) {
        if (std::ranges::find(rows, band.id) == rows.end()) continue;
        DashedRect(ctx, x0, band.y0 + Gui::Dpi::S(1.0F), x1, band.y1 - Gui::Dpi::S(1.0F), accent);
    }
}

}
