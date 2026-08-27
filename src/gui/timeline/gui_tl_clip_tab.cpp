#include "gui_timeline_editor.h"
#include "gui_tl_asset_picker.h"
#include "gui_tl_forms.h"
#include "gui_tl_internal.h"
#include "gui_tl_modals.h"

#include "editor/clip_problems.h"
#include "editor/clip_summary.h"
#include "editor/preset_editor_state.h"
#include "editor/timeline_edits.h"
#include "gui/gui_dpi.h"
#include "imgui.h"
#include "preset/doc/preset_commands.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_enum_names.h"
#include "preset/doc/preset_fields.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <string>

namespace Panels::Timeline {

namespace Doc = Preset::Doc;

namespace {

bool g_gesture = false;

void EndGesture(Editor::State& editor) {
    if (!g_gesture) return;
    editor.EndGesture();
    g_gesture = false;
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

void DrawHeading(const Doc::Track& track, const Doc::Clip& clip) {
    const ImVec2 chip = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(Gui::Dpi::S(10.0F), ImGui::GetTextLineHeight()));
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(chip.x, chip.y + Gui::Dpi::S(2.0F)),
        ImVec2(chip.x + Gui::Dpi::S(6.0F), chip.y + Gui::Dpi::S(16.0F)),
        CommandColor(Doc::TypeOf(clip.command)));
    ImGui::SameLine();
    ImGui::TextUnformatted(
        std::string(Doc::kCommandTypeNames[(std::size_t)Doc::TypeOf(clip.command)]).c_str());
    ImGui::TextDisabled("%s", Editor::ClipSummary(clip, track.target).c_str());
    ImGui::Separator();
    ImGui::TextDisabled("id %s", clip.id.c_str());
    ImGui::TextDisabled("track %s (%s)", track.id.c_str(),
                        std::string(Doc::kTrackKindNames[(std::size_t)track.kind]).c_str());
}

FieldEvent DrawIdentity(Doc::Clip& clip, int length, int duration) {
    FieldEvent event = FieldEvent::None;

    RowLabel("label");
    std::array<char, 128> buffer = {};
    std::copy_n(clip.label.begin(), std::min(clip.label.size(), buffer.size() - 1), buffer.begin());
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("###insp_clip_label", "(none)", buffer.data(), buffer.size())) {
        clip.label = buffer.data();
        event = FieldEvent::Changed;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) event = FieldEvent::Committed;

    int start = clip.start;
    const FieldEvent start_event = DrawIntRow("###insp_clip_start", "start", start);
    if (start_event != FieldEvent::None) {
        const int shift = std::max(0, start) - clip.start;
        clip.start = std::max(0, start);
        if (clip.end.has_value()) *clip.end += shift;
    }
    event = std::max(event, start_event);

    if (!Editor::IsEvent(clip) && clip.end.has_value()) {
        int end = *clip.end;
        const FieldEvent end_event = DrawIntRow("###insp_clip_end", "end (exclusive)", end);
        if (end_event != FieldEvent::None) clip.end = std::max(clip.start + 1, end);
        event = std::max(event, end_event);
    } else if (!Editor::IsEvent(clip)) {
        RowLabel("end");
        ImGui::TextDisabled("open-ended, runs to the document length");
    }

    RowLabel("duration");
    ImGui::TextDisabled("%d frame(s), frames %d..%d", duration, clip.start,
                        Editor::ClipEnd(clip, length) - 1);

    RowLabel("when");
    ImGui::TextDisabled("%s", Editor::GateText(clip).c_str());

    RowLabel("mute");
    if (ImGui::Checkbox("###insp_clip_mute", &clip.muted)) event = FieldEvent::Committed;

    return event;
}

}

void RenderClipTab() {
    Editor::State& editor = Editor::Global();
    if (!editor.Loaded() || editor.Selection().empty()) {
        EndGesture(editor);
        ImGui::TextDisabled("no clip selected");
        ImGui::TextDisabled("Click a clip in the timeline; double-click or Enter opens its "
                            "properties modal.");
        return;
    }

    Doc::Document working = editor.Document();
    const std::string clip_id = editor.Selection().front();
    const Editor::ClipRef ref = Editor::FindClip(working, clip_id);
    if (!ref.Valid()) {
        EndGesture(editor);
        ImGui::TextDisabled("no clip selected");
        return;
    }
    Doc::Track& track = working.tracks[(std::size_t)ref.track];
    Doc::Clip& clip = track.clips[(std::size_t)ref.clip];
    const int length = Editor::DocumentLength(working);
    const int duration = Editor::ClipEnd(clip, length) - clip.start;

    DrawHeading(track, clip);
    const FieldEvent event = DrawIdentity(clip, length, duration);

    if (ImGui::Button("Properties...###insp_clip_props")) RequestClipModal(clip.id, false);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Open the full clip properties modal (Enter on the selected clip).");
    }
    ImGui::Separator();

    if (Doc::FieldsFor(Doc::TypeOf(clip.command)).empty()) {
        Commit(editor, working, event);
        ImGui::TextDisabled("This command has no parameters of its own; it acts on the track "
                            "target through its keys.");
        return;
    }

    const FormContext context = MakeFormContext(working, clip.command, track.target);
    Commit(editor, working, std::max(event, DrawParamForm(context, clip.command)));
}

}
