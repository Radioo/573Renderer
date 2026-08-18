#include "gui_preset_library.h"
#include "gui_preset_library_internal.h"

#include "editor/library_model.h"
#include "editor/preset_editor_state.h"
#include "game_fingerprint.h"
#include "gui_export_panel.h"
#include "gui_icons.h"
#include "gui_widgets.h"
#include "imgui.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_registry.h"
#include "preset/doc/preset_validate.h"
#include "state/app_state.h"
#include "state/preset_commands.h"
#include "timeline/gui_tl_modals.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Panels::PresetLibrary {

namespace Doc = Preset::Doc;

namespace {

char g_filter[128] = {};

std::string Summary(const Doc::Document& document) {
    char buffer[96];
    snprintf(buffer, sizeof(buffer), "%d f, %d marker(s), %d track(s)", document.length.value_or(0),
             (int)document.markers.size(), (int)document.tracks.size());
    return buffer;
}

Editor::LibraryEntry Describe(const Doc::Entry& entry) {
    const Library& state = State();
    Editor::LibraryEntry out;
    out.id = entry.document.id;
    out.name = entry.document.name;
    out.build = entry.document.build;
    out.path = entry.path;
    out.summary = Summary(entry.document);
    if (entry.document.build != state.build_id) {
        out.source = Editor::LibrarySource::OtherBuild;
    } else {
        out.source = entry.builtin ? Editor::LibrarySource::BuiltIn : Editor::LibrarySource::User;
    }
    for (const Doc::Problem& problem : entry.problems) {
        if (problem.severity == Doc::Severity::Error) out.errors++;
        if (problem.severity == Doc::Severity::Warning) out.warnings++;
    }
    const Editor::State& editor = Editor::Global();
    out.loaded = editor.Loaded() && editor.Document().id == entry.document.id &&
                 editor.Document().build == entry.document.build;
    out.modified = out.loaded && editor.Dirty();
    return out;
}

const char* GroupId(Editor::LibrarySource source) {
    switch (source) {
    case Editor::LibrarySource::BuiltIn:
        return "###lib_group_builtin";
    case Editor::LibrarySource::User:
        return "###lib_group_user";
    case Editor::LibrarySource::OtherBuild:
    default:
        return "###lib_group_other";
    }
}

std::string GroupLabel(const Editor::LibraryGroup& group) {
    if (group.source == Editor::LibrarySource::User) {
        return group.label + " (presets/" + State().build_id + "/*.json)" + GroupId(group.source);
    }
    if (group.source == Editor::LibrarySource::OtherBuild) {
        return group.label + " (read-only)" + GroupId(group.source);
    }
    return group.label + GroupId(group.source);
}

void DrawRow(const Editor::LibraryEntry& entry) {
    ImGui::PushID(entry.id.c_str());
    const std::string label = entry.name + "###lib_row_" + entry.id;
    if (ImGui::Selectable(label.c_str(), State().selected == entry.id,
                          ImGuiSelectableFlags_SpanAllColumns)) {
        SelectEntry(entry.id);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s\nid %s, build %s\n%s\n%s", entry.name.c_str(), entry.id.c_str(),
                          entry.build.c_str(), entry.summary.c_str(),
                          entry.path.empty() ? "built into the renderer" : entry.path.c_str());
    }
    ImGui::SameLine();
    if (entry.modified) {
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "modified");
    } else if (entry.errors > 0) {
        ImGui::TextColored(ImVec4(1.0F, 0.45F, 0.45F, 1.0F), "%d error(s)", entry.errors);
    } else if (entry.warnings > 0) {
        ImGui::TextColored(ImVec4(1.0F, 0.85F, 0.3F, 1.0F), "%d warning(s)", entry.warnings);
    } else {
        ImGui::TextDisabled("%s", entry.id.c_str());
    }
    ImGui::PopID();
}

void DrawList(const std::vector<Editor::LibraryEntry>& entries) {
    const std::vector<Editor::LibraryGroup> groups = Editor::GroupLibrary(entries, g_filter);
    if (groups.empty()) {
        ImGui::TextDisabled("No document matches the filter.");
        return;
    }
    for (const Editor::LibraryGroup& group : groups) {
        if (!ImGui::CollapsingHeader(GroupLabel(group).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            continue;
        }
        for (const Editor::LibraryEntry& entry : group.entries)
            DrawRow(entry);
    }
}

void HandleShortcuts() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        return;
    }
    if (!io.KeyCtrl) return;
    if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        RequestAction(io.KeyShift ? Action::SaveAs : Action::Save);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_O, false)) RequestAction(Action::Import);
    if (ImGui::IsKeyPressed(ImGuiKey_N, false)) RequestAction(Action::New);
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) Export::RequestOpen();
}

void DrawHeaderCounts(const std::vector<Editor::LibraryEntry>& entries) {
    int built_in = 0;
    int user = 0;
    for (const Editor::LibraryEntry& entry : entries) {
        if (entry.source == Editor::LibrarySource::BuiltIn) built_in++;
        if (entry.source == Editor::LibrarySource::User) user++;
    }
    char suffix[80];
    snprintf(suffix, sizeof(suffix), "(%d built-in, %d user)", built_in, user);
    Gui::SectionHeader(ICON_SCENE, "Preset library", suffix);
}

void DrawBuildLine() {
    const Library& state = State();
    const std::string read_only = ReadOnlyReason();
    if (read_only.empty()) {
        ImGui::TextDisabled("%s", state.build_name.c_str());
        return;
    }
    ImGui::TextColored(ImVec4(1.0F, 0.85F, 0.3F, 1.0F), "%s", read_only.c_str());
    if (!ImGui::IsItemHovered()) return;
    ImGui::SetTooltip("This document is for another build, so it is shown but never loaded "
                      "into the renderer and every edit is refused.\nDuplicate it to get "
                      "an editable copy for %s.",
                      state.build_name.c_str());
}

void DrawBody(const std::vector<Editor::LibraryEntry>& entries) {
    const float line = ImGui::GetTextLineHeightWithSpacing();
    const float body = std::max(line * 2.0F, ImGui::GetContentRegionAvail().y -
                                                 (ImGui::GetStyle().ItemSpacing.y * 2.0F) - 1.0F);
    const float bar = ImGui::GetStyle().ScrollbarSize;
    const float problems_h = std::clamp(body * 0.3F, line + bar, (line * 3.0F) + bar);
    const float list_h = std::max(line, body - problems_h);
    ImGui::BeginChild("lib_scroll", ImVec2(0, list_h), 0, ImGuiWindowFlags_HorizontalScrollbar);
    DrawList(entries);
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::BeginChild("lib_problems", ImVec2(0, 0), 0, ImGuiWindowFlags_HorizontalScrollbar);
    DrawProblems();
    ImGui::EndChild();
}

void PumpPending() {
    Library& state = State();
    if (state.new_document_pending) {
        state.new_document_pending = false;
        Timeline::RequestNewDocumentModal(TakenIds(state.build_id));
    }
    if (state.pending_action == Action::None) return;
    const Action action = state.pending_action;
    state.pending_action = Action::None;
    RunAction(action);
}

}

Library& State() {
    static Library state;
    return state;
}

void SetUserRoot(const std::string& root) {
    Library& state = State();
    state.root_override = root;
    state.scanned = false;
    state.scanned_dir.clear();
}

std::filesystem::path Root() {
    const Library& state = State();
    if (!state.root_override.empty()) return {state.root_override};
    return Doc::UserRoot();
}

std::vector<Editor::LibraryEntry> Entries() {
    std::vector<Editor::LibraryEntry> out;
    for (const Doc::Entry* entry : State().registry.All())
        out.push_back(Describe(*entry));
    return out;
}

std::vector<std::string> TakenIds(const std::string& build) {
    std::vector<std::string> ids;
    for (const Doc::Entry* entry : State().registry.All()) {
        if (entry->document.build == build) ids.push_back(entry->document.id);
    }
    return ids;
}

const Doc::Entry* FindEntry(const std::string& id) {
    for (const Doc::Entry* entry : State().registry.All()) {
        if (entry->document.id == id) return entry;
    }
    return nullptr;
}

bool ForeignBuild(const Doc::Document& document) {
    const Library& state = State();
    return !state.build_id.empty() && document.build != state.build_id;
}

std::string ReadOnlyReason() {
    const Editor::State& editor = Editor::Global();
    if (!editor.Loaded() || !editor.ReadOnly()) return {};
    return "read-only: built for " + editor.Document().build;
}

bool IsBuiltInId(const std::string& build, const std::string& id) {
    const std::vector<const Doc::Entry*> entries = State().registry.All();
    return std::ranges::any_of(entries, [&build, &id](const Doc::Entry* entry) {
        return entry->builtin && entry->document.build == build && entry->document.id == id;
    });
}

void Rescan(bool report_progress) {
    Library& state = State();
    App::State& app = App::Global();
    Doc::ScanProgressFn progress;
    if (report_progress) {
        app.BeginLoad("Scene presets");
        progress = [&app](const Doc::ScanStatus& status) {
            if (status.total <= 0) return;
            char stage[320];
            snprintf(stage, sizeof(stage), "%s (%d / %d)",
                     std::filesystem::path(status.current).filename().string().c_str(), status.done,
                     status.total);
            app.UpdateLoadStage(stage, (float)status.done / (float)status.total);
        };
    }
    state.registry.Load(Root(), progress);
    if (report_progress) app.EndLoad();
    state.scanned = true;
}

void OpenDocument(const Doc::Document& document, bool from_builtin) {
    Library& state = State();
    const bool foreign = ForeignBuild(document);
    Editor::Global().LoadDocument(document, foreign);
    state.selected = document.id;
    state.loaded_builtin = from_builtin;
    if (foreign) return;
    auto shared = std::make_shared<const Doc::Document>(document);
    App::Global().PostCommand(PresetCmd::Wrap(
        PresetCmd::LoadDocument{.document = shared, .game_dir = App::Global().GameDir()}));
}

void SelectEntry(const std::string& id) {
    if (Editor::Global().Loaded() && Editor::Global().Dirty() &&
        Editor::Global().Document().id != id) {
        RequestPrompt(Action::None, id);
        return;
    }
    const Doc::Entry* entry = FindEntry(id);
    if (entry == nullptr) return;
    OpenDocument(entry->document, entry->builtin);
}

bool Active() {
    return App::Global().ActiveBackendId() == "scene3d";
}

void Render() {
    if (!Active()) return;
    Library& state = State();
    const std::string dir = App::Global().GameDir();
    if (!state.scanned || dir != state.scanned_dir) {
        state.scanned_dir = dir;
        const GameFingerprint::Match match = GameFingerprint::Identify(dir);
        state.build_id = match.build != nullptr ? match.build->id : std::string{};
        state.build_name = match.build != nullptr ? match.build->name : std::string{};
        state.scanned = true;
        if (!state.build_id.empty()) Rescan(true);
    }

    const std::vector<Editor::LibraryEntry> entries = Entries();
    DrawHeaderCounts(entries);

    if (state.build_id.empty()) {
        ImGui::TextDisabled("No known build fingerprint under this directory.");
        ImGui::TextDisabled("Presets are keyed by build, so nothing can be listed for it.");
        return;
    }
    DrawBuildLine();

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("###lib_filter", "filter presets...", g_filter, sizeof(g_filter));

    DrawActions();
    ImGui::Separator();
    DrawBody(entries);
    HandleShortcuts();
    PumpPending();
}

void RequestAction(Action action) {
    Library& state = State();
    if (action == Action::None) return;
    const bool destructive = action == Action::New || action == Action::Import ||
                             action == Action::Revert || action == Action::Reset;
    if (destructive && Editor::Global().Loaded() && Editor::Global().Dirty()) {
        RequestPrompt(action, {});
        return;
    }
    state.pending_action = action;
}

}
