#include "gui_tl_modals.h"

#include "editor/options_model.h"
#include "editor/preset_editor_state.h"
#include "gui_tl_forms.h"
#include "imgui.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

constexpr const char* kTitle = "Option properties";
constexpr float kScreenMargin = 60.0F;
constexpr std::array<Doc::Ease, 5> kTransitionEases = {Doc::Ease::Hold, Doc::Ease::Linear,
                                                       Doc::Ease::EaseIn, Doc::Ease::EaseOut,
                                                       Doc::Ease::EaseInOut};

int g_option = -1;
int g_choice = 0;
int g_base_undo = 0;
int g_target = 0;
bool g_open_requested = false;
bool g_gesture = false;

FieldEvent DrawTextRow(const char* id, const char* label, std::string& value) {
    RowLabel(label);
    std::array<char, 96> buffer = {};
    std::copy_n(value.begin(), std::min(value.size(), buffer.size() - 1), buffer.begin());
    ImGui::SetNextItemWidth(-30.0F);
    FieldEvent event = FieldEvent::None;
    if (ImGui::InputText(id, buffer.data(), buffer.size())) {
        value = buffer.data();
        event = FieldEvent::Changed;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) event = FieldEvent::Committed;
    return event;
}

bool IsVectorTarget(const std::string& id) {
    constexpr std::array<std::string_view, 9> kVectorFields = {
        "position", "rotation", "scale", "eye", "at", "up", "direction", "diffuse", "specular"};
    return std::ranges::any_of(kVectorFields, [&id](std::string_view field) {
        return id.ends_with("." + std::string(field));
    });
}

FieldEvent DrawTransition(Doc::Transition& transition) {
    ImGui::SeparatorText("Transition");
    FieldEvent event = DrawIntRow("###tl_option_frames", "frames", transition.frames);
    transition.frames = std::max(0, transition.frames);
    event = std::max(event, DrawIntRow("###tl_option_step", "step per frame", transition.step));
    transition.step = std::max(1, transition.step);

    std::vector<std::string> names;
    int selected = 0;
    for (std::size_t i = 0; i < kTransitionEases.size(); i++) {
        names.emplace_back(Doc::kEaseNames[(std::size_t)kTransitionEases[i]]);
        if (kTransitionEases[i] == transition.ease) selected = (int)i;
    }
    const FieldEvent ease = DrawEnumRow("###tl_option_ease", "ease", selected, names);
    if (ease != FieldEvent::None) transition.ease = kTransitionEases[(std::size_t)selected];
    event = std::max(event, ease);

    event = std::max(event, DrawDoubleRow("###tl_option_kick", "spin kick", transition.spin_kick,
                                          0.1F, "x spin"));
    ImGui::TextDisabled("applied on a choice change to the models this option moves; it decays by "
                        "each model's motion clip spin_kick_decay");
    return event;
}

FieldEvent DrawChoiceValues(const Doc::Document& document, Doc::ChoiceSpec& choice) {
    FieldEvent event = FieldEvent::None;
    for (std::size_t i = 0; i < choice.values.size(); i++) {
        Doc::ChoiceValue& value = choice.values[i];
        const std::string suffix = std::to_string(i);
        RowLabel(value.id.c_str());
        ImGui::SetNextItemWidth(-60.0F);
        if (auto* vector = std::get_if<Doc::Vec3>(&value.value)) {
            if (ImGui::DragScalarN(("###tl_option_value_" + suffix).c_str(), ImGuiDataType_Double,
                                   vector->data(), 3, 0.01F)) {
                event = std::max(event, FieldEvent::Changed);
            }
        } else if (auto* scalar = std::get_if<double>(&value.value)) {
            if (ImGui::DragScalar(("###tl_option_value_" + suffix).c_str(), ImGuiDataType_Double,
                                  scalar, 0.01F)) {
                event = std::max(event, FieldEvent::Changed);
            }
        } else {
            ImGui::TextDisabled("(unsupported value kind)");
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) event = FieldEvent::Committed;
        ImGui::SameLine();
        if (!ImGui::SmallButton(("x###tl_option_value_remove_" + suffix).c_str())) continue;
        choice.values.erase(choice.values.begin() + (long long)i);
        return FieldEvent::Committed;
    }

    const std::vector<std::string> targets = Editor::ChoiceValueTargets(document);
    if (targets.empty()) return event;
    g_target = std::clamp(g_target, 0, (int)targets.size() - 1);
    DrawEnumRow("###tl_option_value_target", "target", g_target, targets);
    RowLabel("");
    if (!ImGui::Button("+ add target value###tl_option_add_value")) return event;
    const std::string& id = targets[(std::size_t)g_target];
    const bool known = std::ranges::any_of(
        choice.values, [&id](const Doc::ChoiceValue& value) { return value.id == id; });
    if (known) return event;
    choice.values.push_back(Doc::ChoiceValue{
        .id = id,
        .value = IsVectorTarget(id) ? Doc::OverrideValue{Doc::Vec3{}} : Doc::OverrideValue{0.0}});
    return FieldEvent::Committed;
}

FieldEvent DrawChoices(Doc::Document& document, Doc::OptionSpec& option) {
    ImGui::SeparatorText("Choices and target values");
    FieldEvent event = FieldEvent::None;
    for (std::size_t i = 0; i < option.choices.size(); i++) {
        const std::string suffix = std::to_string(i);
        if (ImGui::RadioButton(("###tl_option_choice_" + suffix).c_str(),
                               std::cmp_equal(i, g_choice))) {
            g_choice = (int)i;
        }
        ImGui::SameLine();
        std::string label = option.choices[i].label;
        ImGui::SetNextItemWidth(220.0F);
        std::array<char, 64> buffer = {};
        std::copy_n(label.begin(), std::min(label.size(), buffer.size() - 1), buffer.begin());
        if (ImGui::InputText(("###tl_option_choice_label_" + suffix).c_str(), buffer.data(),
                             buffer.size())) {
            Editor::RenameChoice(document, g_option, (int)i, buffer.data());
            event = std::max(event, FieldEvent::Changed);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) event = FieldEvent::Committed;
        ImGui::SameLine();
        ImGui::TextDisabled("%zu value(s)", option.choices[i].values.size());
        ImGui::SameLine();
        if (ImGui::SmallButton(("up###tl_option_choice_up_" + suffix).c_str()) &&
            Editor::MoveChoice(document, g_option, (int)i, -1)) {
            if (std::cmp_equal(i, g_choice)) g_choice--;
            return FieldEvent::Committed;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(("down###tl_option_choice_down_" + suffix).c_str()) &&
            Editor::MoveChoice(document, g_option, (int)i, 1)) {
            if (std::cmp_equal(i, g_choice)) g_choice++;
            return FieldEvent::Committed;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(("x###tl_option_choice_remove_" + suffix).c_str()) &&
            option.choices.size() > 1) {
            option.choices.erase(option.choices.begin() + (long long)i);
            return FieldEvent::Committed;
        }
    }
    if (ImGui::Button("+ choice###tl_option_add_choice")) {
        option.choices.push_back(
            Doc::ChoiceSpec{.label = "Choice " + std::to_string(option.choices.size() + 1)});
        return FieldEvent::Committed;
    }
    g_choice = std::clamp(g_choice, 0, (int)option.choices.size() - 1);
    ImGui::SeparatorText(
        ("Selected choice: " + option.choices[(std::size_t)g_choice].label).c_str());
    return std::max(event, DrawChoiceValues(document, option.choices[(std::size_t)g_choice]));
}

FieldEvent DrawBody(Doc::Document& working) {
    Doc::OptionSpec& option = working.options[(std::size_t)g_option];
    FieldEvent event = DrawTextRow("###tl_option_id", "id", option.id);
    event = std::max(event, DrawTextRow("###tl_option_label", "label", option.label));

    std::vector<std::string> labels;
    labels.reserve(option.choices.size());
    for (const Doc::ChoiceSpec& choice : option.choices)
        labels.push_back(choice.label);
    if (!labels.empty()) {
        int selected = std::clamp(option.default_choice, 0, (int)labels.size() - 1);
        const FieldEvent picked =
            DrawEnumRow("###tl_option_default", "default choice", selected, labels);
        if (picked != FieldEvent::None) option.default_choice = selected;
        event = std::max(event, picked);
    }
    event = std::max(event, DrawChoices(working, working.options[(std::size_t)g_option]));
    return std::max(event, DrawTransition(working.options[(std::size_t)g_option].transition));
}

void Commit(Editor::State& editor, const Doc::Document& working, FieldEvent event) {
    if (event == FieldEvent::None) return;
    if (event == FieldEvent::Changed && !g_gesture) {
        editor.BeginGesture();
        g_gesture = true;
    }
    editor.Apply([&working](Doc::Document& document) {
        document = working;
        return true;
    });
    if (event == FieldEvent::Committed && g_gesture) {
        editor.EndGesture();
        g_gesture = false;
    }
}

void Close() {
    if (g_gesture) {
        Editor::Global().EndGesture();
        g_gesture = false;
    }
    g_option = -1;
    ImGui::CloseCurrentPopup();
}

}

void ResetOptionModal() {
    if (g_gesture) {
        Editor::Global().EndGesture();
        g_gesture = false;
    }
    g_option = -1;
    g_choice = 0;
    g_open_requested = false;
}

void RequestOptionModal(int index) {
    g_option = index;
    g_choice = 0;
    g_target = 0;
    g_base_undo = Editor::Global().UndoDepth();
    g_open_requested = true;
}

void RenderOptionModal() {
    Editor::State& editor = Editor::Global();
    if (g_open_requested) {
        ImGui::OpenPopup(kTitle);
        g_open_requested = false;
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
    ImGui::SetNextWindowSizeConstraints(ImVec2(660.0F, 0.0F),
                                        ImVec2(660.0F, viewport->WorkSize.y - kScreenMargin));
    if (!ImGui::BeginPopupModal(kTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (!editor.Loaded() || g_option < 0 ||
        std::cmp_greater_equal(g_option, editor.Document().options.size())) {
        Close();
        ImGui::EndPopup();
        return;
    }

    Doc::Document working = editor.Document();
    ImGui::Text("option %s", working.options[(std::size_t)g_option].id.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("%s", working.id.c_str());
    ImGui::Separator();

    const FieldEvent event = DrawBody(working);
    Commit(editor, working, event);

    ImGui::Separator();
    const bool escape =
        !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape, false);
    if (ImGui::Button("Cancel###tl_option_cancel") || escape) {
        while (editor.UndoDepth() > g_base_undo)
            editor.Undo();
        editor.ClearRedo();
        Close();
        ImGui::EndPopup();
        return;
    }
    ImGui::SameLine();
    if (ImGui::Button("Done###tl_option_done")) {
        editor.CollapseUndo(g_base_undo);
        Close();
    }
    ImGui::EndPopup();
}

}
