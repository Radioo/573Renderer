#include "gui_tl_modals.h"

#include "editor/clip_summary.h"
#include "editor/preset_editor_state.h"
#include "editor/timeline_edits.h"
#include "gui_tl_asset_picker.h"
#include "gui/gui_dpi.h"
#include "gui_tl_forms.h"
#include "gui_tl_preview.h"
#include "imgui.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_fields.h"
#include "preset/preset_preview.h"
#include "state/app_state.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

constexpr const char* kTitle = "Clip properties";
constexpr float kModalWidth = 660.0F;
constexpr float kScreenMargin = 60.0F;
constexpr float kModalChrome = 150.0F;

std::string g_clip_id;
int g_base_undo = 0;
bool g_open_requested = false;
bool g_gesture = false;
bool g_release_preview = false;
bool g_focus_tween = false;
float g_body_h = 0.0F;

Doc::Clip* MutableClip(Doc::Document& document, const std::string& clip_id) {
    const Editor::ClipRef ref = Editor::FindClip(document, clip_id);
    if (!ref.Valid()) return nullptr;
    return &document.tracks[(std::size_t)ref.track].clips[(std::size_t)ref.clip];
}

std::string TrackOf(const Doc::Document& document, const std::string& clip_id) {
    const Editor::ClipRef ref = Editor::FindClip(document, clip_id);
    if (!ref.Valid()) return {};
    return document.tracks[(std::size_t)ref.track].id;
}

FieldEvent DrawGateChoices(const Doc::Document& document, Doc::Gate& gate) {
    std::vector<std::string> labels;
    for (const Doc::OptionSpec& option : document.options) {
        if (option.id != gate.option) continue;
        for (const Doc::ChoiceSpec& choice : option.choices)
            labels.push_back(choice.label);
    }
    RowLabel("choice(s)");
    if (labels.empty()) {
        ImGui::TextDisabled("the option declares no choices");
        return FieldEvent::None;
    }
    FieldEvent event = FieldEvent::None;
    for (const std::string& label : labels) {
        bool on = std::ranges::find(gate.choices, label) != gate.choices.end();
        if (!ImGui::Checkbox(("###tl_general_gate_choice_" + label).c_str(), &on)) {
            ImGui::SameLine();
            ImGui::TextUnformatted(label.c_str());
            continue;
        }
        if (on) {
            gate.choices.push_back(label);
        } else {
            std::erase(gate.choices, label);
        }
        event = FieldEvent::Committed;
        ImGui::SameLine();
        ImGui::TextUnformatted(label.c_str());
    }
    return event;
}

FieldEvent DrawGate(const Doc::Document& document, Doc::Clip& clip) {
    std::vector<std::string> options = {"(always)"};
    for (const Doc::OptionSpec& option : document.options)
        options.push_back(option.id);
    int selected = 0;
    if (clip.when.has_value()) {
        for (std::size_t i = 1; i < options.size(); i++) {
            if (options[i] == clip.when->option) selected = (int)i;
        }
    }
    FieldEvent event = DrawEnumRow("###tl_general_gate_option", "when option", selected, options);
    if (event != FieldEvent::None) {
        if (selected == 0) {
            clip.when.reset();
        } else {
            Doc::Gate gate;
            gate.option = options[(std::size_t)selected];
            clip.when = gate;
        }
    }
    if (!clip.when.has_value()) return event;

    int kind = (int)clip.when->kind;
    std::vector<std::string> kinds;
    kinds.reserve(Doc::kGateKindNames.size());
    for (const std::string_view name : Doc::kGateKindNames)
        kinds.emplace_back(name);
    const FieldEvent kind_event = DrawEnumRow("###tl_general_gate_kind", "matches", kind, kinds);
    if (kind_event != FieldEvent::None) clip.when->kind = (Doc::GateKind)kind;
    event = std::max(event, kind_event);

    return std::max(event, DrawGateChoices(document, *clip.when));
}

FieldEvent DrawExtent(Doc::Clip& clip, int length) {
    int start = clip.start;
    FieldEvent event = DrawIntRow("###tl_general_start", "start", start);
    if (event != FieldEvent::None) {
        const int shift = std::max(0, start) - clip.start;
        clip.start = std::max(0, start);
        if (clip.end.has_value()) *clip.end += shift;
    }
    if (Editor::IsEvent(clip)) return event;

    bool open_ended = !clip.end.has_value();
    RowLabel("open-ended");
    if (ImGui::Checkbox("###tl_general_open", &open_ended)) {
        clip.end = open_ended ? std::optional<int>{}
                              : std::optional<int>{std::min(length, clip.start + 60)};
        event = FieldEvent::Committed;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("runs to the document length");
    if (open_ended) return event;

    int end = clip.end.value_or(clip.start + 1);
    const FieldEvent end_event = DrawIntRow("###tl_general_end", "end (exclusive)", end);
    if (end_event != FieldEvent::None) clip.end = std::max(clip.start + 1, end);
    return std::max(event, end_event);
}

FieldEvent DrawGeneral(Doc::Document& document, Doc::Clip& clip, int length, int fps) {
    FieldEvent event = FieldEvent::None;

    RowLabel("label");
    std::array<char, 128> buffer = {};
    std::copy_n(clip.label.begin(), std::min(clip.label.size(), buffer.size() - 1), buffer.begin());
    ImGui::SetNextItemWidth(Gui::Dpi::S(-30.0F));
    if (ImGui::InputText("###tl_general_label", buffer.data(), buffer.size())) {
        clip.label = buffer.data();
        event = FieldEvent::Changed;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) event = FieldEvent::Committed;

    RowLabel("id");
    ImGui::TextDisabled("%s (read-only)", clip.id.c_str());

    event = std::max(event, DrawExtent(clip, length));

    const int duration = Editor::ClipEnd(clip, length) - clip.start;
    RowLabel("duration");
    ImGui::TextDisabled("%d frames, %.2f s", duration, (double)duration / (double)std::max(1, fps));

    std::vector<std::string> tracks;
    const std::string home = TrackOf(document, clip.id);
    const Editor::ClipRef ref = Editor::FindClip(document, clip.id);
    const Doc::TrackKind kind = document.tracks[(std::size_t)ref.track].kind;
    int track_index = 0;
    for (const Doc::Track& track : document.tracks) {
        if (track.kind != kind) continue;
        if (track.id == home) track_index = (int)tracks.size();
        tracks.push_back(track.id);
    }
    const FieldEvent track_event = DrawEnumRow("###tl_general_track", "track", track_index, tracks);
    if (track_event != FieldEvent::None && tracks[(std::size_t)track_index] != home) {
        Editor::MoveClipToTrack(document, clip.id, tracks[(std::size_t)track_index], clip.start);
        return FieldEvent::Committed;
    }

    Doc::Clip* live = MutableClip(document, clip.id);
    if (live == nullptr) return event;
    event = std::max(event, DrawGate(document, *live));

    RowLabel("mute");
    if (ImGui::Checkbox("###tl_general_mute", &live->muted)) event = FieldEvent::Committed;
    ImGui::SameLine();
    ImGui::TextDisabled("the evaluator skips this clip");
    return event;
}

void DrawParams(const Doc::Document& document, Doc::Clip& clip, const std::string& target,
                FieldEvent& event) {
    const FormContext context = MakeFormContext(document, clip.command, target);
    event = std::max(event, DrawParamForm(context, clip.command));

    const auto* animate = std::get_if<Doc::SpriteAnimate>(&clip.command);
    if (animate != nullptr && !animate->animation.empty()) {
        ImGui::Separator();
        RequestPreview(animate->asset, animate->animation, animate->hidden_parts);
        DrawPreviewStrip(
            Preset::Preview::KeyFor(animate->asset, animate->animation, animate->hidden_parts));
    }
    event = std::max(event, DrawHiddenParts(context, clip.command));
}

bool HasTweenTab(Doc::CommandType type) {
    return std::ranges::any_of(Doc::KeyFieldsFor(type),
                               [](const Doc::FieldDesc& field) { return field.tweenable; });
}

FieldEvent DrawTabs(Doc::Document& working, Doc::CommandType type, const std::string& target) {
    FieldEvent event = FieldEvent::None;
    if (!ImGui::BeginTabBar("###tl_clip_tabs")) return event;

    Doc::Clip* clip = MutableClip(working, g_clip_id);
    if (clip != nullptr && ImGui::BeginTabItem("General###tl_tab_general")) {
        event = std::max(event,
                         DrawGeneral(working, *clip, Editor::DocumentLength(working), working.fps));
        ImGui::EndTabItem();
    }
    clip = MutableClip(working, g_clip_id);
    if (clip != nullptr && !Doc::FieldsFor(type).empty() &&
        ImGui::BeginTabItem("Params###tl_tab_params")) {
        DrawParams(working, *clip, target, event);
        ImGui::EndTabItem();
    }
    clip = MutableClip(working, g_clip_id);
    ImGuiTabItemFlags tween_flags = ImGuiTabItemFlags_None;
    if (g_focus_tween) {
        tween_flags = ImGuiTabItemFlags_SetSelected;
        g_focus_tween = false;
    }
    if (clip != nullptr && HasTweenTab(type) &&
        ImGui::BeginTabItem("Tween###tl_tab_tween", nullptr, tween_flags)) {
        event = std::max(event,
                         DrawTweenTab(working, g_clip_id, App::Global().GetPresetStatus().frame));
        ImGui::EndTabItem();
    }
    clip = MutableClip(working, g_clip_id);
    if (clip != nullptr && HasAssetField(type) && ImGui::BeginTabItem("Asset###tl_tab_asset")) {
        event = std::max(event, DrawAssetTab(MakeFormContext(working, clip->command, target),
                                             working, clip->command));
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
    return event;
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
    g_clip_id.clear();
    g_release_preview = true;
    ImGui::CloseCurrentPopup();
}

void Cancel(Editor::State& editor) {
    while (editor.UndoDepth() > g_base_undo)
        editor.Undo();
    editor.ClearRedo();
    Close();
}

bool CancelKeyPressed() {
    return ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
           !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape, false);
}

FieldEvent DrawBody(Doc::Document& working, Doc::CommandType type, const std::string& target) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float cap = std::max(Gui::Dpi::S(200.0F),
                               viewport->WorkSize.y - Gui::Dpi::S(kScreenMargin + kModalChrome));
    const float height = (g_body_h <= 0.0F) ? cap : std::clamp(g_body_h, Gui::Dpi::S(60.0F), cap);
    FieldEvent event = FieldEvent::None;
    if (ImGui::BeginChild("###tl_clip_body", ImVec2(0.0F, height), ImGuiChildFlags_None)) {
        event = DrawTabs(working, type, target);
        g_body_h = ImGui::GetCursorPosY() + (2.0F * ImGui::GetStyle().WindowPadding.y);
    }
    ImGui::EndChild();
    return event;
}

void DrawFooter(Editor::State& editor) {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false) && editor.UndoDepth() > g_base_undo) {
        editor.Undo();
    }
    ImGui::Separator();
    ImGui::TextDisabled("Ctrl+Z inside the modal undoes field by field. Esc cancels.");
    ImGui::SameLine();
    if (ImGui::Button("Cancel###tl_clip_cancel") || CancelKeyPressed()) {
        Cancel(editor);
        return;
    }
    ImGui::SameLine();
    if (ImGui::Button("Done###tl_clip_done")) {
        editor.CollapseUndo(g_base_undo);
        Close();
    }
}

}

void ResetClipModal() {
    if (g_gesture) {
        Editor::Global().EndGesture();
        g_gesture = false;
    }
    g_clip_id.clear();
    g_open_requested = false;
    g_release_preview = false;
    ForgetPreview();
}

void RequestClipModal(std::string clip_id, bool focus_tween) {
    g_body_h = 0.0F;
    g_clip_id = std::move(clip_id);
    g_base_undo = Editor::Global().UndoDepth();
    g_open_requested = true;
    g_focus_tween = focus_tween;
}

void RenderClipModal() {
    Editor::State& editor = Editor::Global();
    if (g_release_preview) {
        ForgetPreview();
        g_release_preview = false;
    }
    if (g_open_requested) {
        ImGui::OpenPopup(kTitle);
        g_open_requested = false;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
    ImGui::SetNextWindowSizeConstraints(
        Gui::Dpi::S(kModalWidth, 0.0F),
        ImVec2(Gui::Dpi::S(kModalWidth), viewport->WorkSize.y - Gui::Dpi::S(kScreenMargin)));
    if (!ImGui::BeginPopupModal(kTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (!editor.Loaded()) {
        Close();
        ImGui::EndPopup();
        return;
    }

    Doc::Document working = editor.Document();
    Doc::Clip* clip = MutableClip(working, g_clip_id);
    if (clip == nullptr) {
        Close();
        ImGui::EndPopup();
        return;
    }

    const Doc::CommandType type = Doc::TypeOf(clip->command);
    const std::string track = TrackOf(working, clip->id);
    const int track_index = Editor::FindTrack(working, track);
    const std::string target =
        track_index >= 0 ? working.tracks[(std::size_t)track_index].target : std::string{};
    ImGui::Text("%s  %s", std::string(Doc::kCommandTypeNames[(std::size_t)type]).c_str(),
                Editor::ClipSummary(*clip, target).c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("clip %s", clip->id.c_str());
    ImGui::Separator();

    Commit(editor, working, DrawBody(working, type, target));
    if (editor.PendingRequest().kind == Editor::RequestKind::CurveEditor) {
        editor.CollapseUndo(g_base_undo);
        Close();
        ImGui::EndPopup();
        return;
    }
    DrawFooter(editor);
    ImGui::EndPopup();
}

}
