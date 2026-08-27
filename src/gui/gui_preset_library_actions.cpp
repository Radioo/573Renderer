#include "gui_preset_library_internal.h"

#include "editor/library_model.h"
#include "gui_preset_library.h"
#include "editor/preset_editor_state.h"
#include "editor/timeline_edits.h"
#include "imgui.h"
#include "native_dialog.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_json.h"
#include "preset/doc/preset_registry.h"
#include "preset/doc/preset_validate.h"
#include "state/app_state.h"
#include "timeline/gui_tl_modals.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace Panels::PresetLibrary {

namespace Doc = Preset::Doc;

namespace {

constexpr const char* kPromptTitle = "Unsaved changes";
constexpr const char* kImportTitle = "Import problems";
constexpr const char* kSaveAsTitle = "Save as user copy";

bool g_prompt_open = false;
bool g_import_open = false;
bool g_saveas_open = false;
bool g_import_blocked = true;
std::string g_import_path;
std::string g_import_message;
std::array<char, 96> g_saveas_id = {};

const Doc::Document* Loaded() {
    const Editor::State& editor = Editor::Global();
    return editor.Loaded() ? &editor.Document() : nullptr;
}

bool WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::error_code code;
    std::filesystem::create_directories(path.parent_path(), code);
    std::ofstream file(path, std::ios::binary);
    if (!file.good()) return false;
    file << text;
    return file.good();
}

std::filesystem::path UserPath(const Doc::Document& document) {
    return Root() / Editor::UserRelativePath(document.build, document.id);
}

NativeDialog::FileRequest JsonRequest(const std::string& title, const std::string& name) {
    return NativeDialog::FileRequest{.title = title,
                                     .filter_label = "Scene preset JSON",
                                     .filter_pattern = "*.json",
                                     .extension = "json",
                                     .initial_dir = Root().string(),
                                     .initial_name = name};
}

void SaveLoadedTo(const Doc::Document& document) {
    if (!WriteFile(UserPath(document), Doc::Save(document))) return;
    Editor::Global().MarkSaved();
    Rescan(false);
    State().selected = document.id;
}

void DoSave() {
    const Doc::Document* document = Loaded();
    if (document == nullptr) return;
    if (IsBuiltInId(document->build, document->id)) {
        RequestSaveAs();
        return;
    }
    SaveLoadedTo(*document);
}

void DoNew() {
    Library& state = State();
    Doc::Document document;
    document.build = state.build_id;
    document.name = "New preset";
    document.id = Editor::UniqueId("new-preset", TakenIds(state.build_id));
    document.length = 600;
    OpenDocument(document, false);
    state.new_document_pending = true;
}

void DoDuplicate() {
    const Doc::Document* source = Loaded();
    if (source == nullptr) return;
    Doc::Document copy = *source;
    const std::string build = ForeignBuild(*source) ? State().build_id : source->build;
    copy.build = build;
    const std::string id = Editor::CopyId(source->id, TakenIds(build));
    OpenDocument(copy, false);
    Editor::Global().Apply([&id](Doc::Document& document) {
        document.id = id;
        document.name += " copy";
        return true;
    });
    State().selected = id;
}

bool ClashesWithAnotherUserFile(const Doc::Document& document, const std::string& source) {
    const Doc::Entry* owner = State().registry.Find(document.build, document.id);
    if (owner == nullptr || owner->builtin || owner->path.empty()) return false;
    std::error_code code;
    return !std::filesystem::equivalent(owner->path, source, code);
}

void DoImport() {
    const std::string path =
        NativeDialog::OpenFile(nullptr, JsonRequest("Import a scene preset", {}));
    if (path.empty()) return;
    const Doc::Loaded loaded = Doc::LoadFile(path);
    if (!loaded.has_value()) {
        RequestImportProblems(path, loaded.error().message, true);
        return;
    }
    Doc::Document document = *loaded;
    const std::string wanted = document.id;
    if (ClashesWithAnotherUserFile(document, path))
        document.id = Editor::UniqueId(wanted, TakenIds(document.build));
    const bool renamed = document.id != wanted;
    OpenDocument(document, false);
    if (document.build != State().build_id) return;
    if (!renamed && IsBuiltInId(document.build, document.id)) return;
    SaveLoadedTo(document);
    if (!renamed) return;
    RequestImportProblems(path,
                          "id \"" + wanted + "\" already belongs to another document of this " +
                              "build, so the file was imported as \"" + document.id + "\".",
                          false);
}

void DoExport() {
    const Doc::Document* document = Loaded();
    if (document == nullptr) return;
    const std::string path = NativeDialog::SaveFile(
        nullptr, JsonRequest("Export a scene preset", document->id + ".json"));
    if (path.empty()) return;
    WriteFile(path, Doc::Save(*document));
}

void DoRevert(bool built_in_only) {
    const Doc::Document* document = Loaded();
    if (document == nullptr) return;
    for (const Doc::Entry* entry : State().registry.All()) {
        if (entry->document.id != document->id || entry->document.build != document->build)
            continue;
        if (built_in_only && !entry->builtin) continue;
        OpenDocument(entry->document, entry->builtin);
        return;
    }
}

void ClosePrompt() {
    g_prompt_open = false;
    State().prompted_action = Action::None;
    State().pending_select.clear();
    App::Global().ClearCloseRequest();
}

void PumpCloseRequest() {
    App::State& app = App::Global();
    const Editor::State& editor = Editor::Global();
    const bool unsaved = editor.Loaded() && editor.Dirty();
    app.SetCloseNeedsPrompt(unsaved);
    if (!app.CloseRequested()) return;
    if (!unsaved) {
        app.ConfirmClose();
        return;
    }
    if (State().prompted_action == Action::Quit) return;
    RequestPrompt(Action::Quit, {});
}

void ApplyPrompted() {
    Library& state = State();
    const Action action = state.prompted_action;
    const std::string select = state.pending_select;
    ClosePrompt();
    if (action == Action::Quit) {
        App::Global().ConfirmClose();
        return;
    }
    if (!select.empty()) {
        const Doc::Entry* entry = FindEntry(select);
        if (entry != nullptr) OpenDocument(entry->document, entry->builtin);
        return;
    }
    state.pending_action = action;
}

void RenderPrompt() {
    if (g_prompt_open) ImGui::OpenPopup(kPromptTitle);
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
    if (!ImGui::BeginPopupModal(kPromptTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        g_prompt_open = false;
        return;
    }
    g_prompt_open = false;
    const Doc::Document* document = Loaded();
    ImGui::TextUnformatted(document != nullptr
                               ? ("\"" + document->name + "\" has unsaved changes.").c_str()
                               : "The document has unsaved changes.");
    if (State().prompted_action == Action::Quit) {
        ImGui::TextDisabled("The renderer is closing. Cancel keeps it running.");
    }
    const bool built_in = document != nullptr && IsBuiltInId(document->build, document->id);
    if (built_in) {
        ImGui::TextDisabled("Built-in ids are reserved, so Save asks for a user copy id first.");
    } else {
        ImGui::TextDisabled("Saving writes presets/%s/%s.json next to the renderer.",
                            document != nullptr ? document->build.c_str() : "",
                            document != nullptr ? document->id.c_str() : "");
    }
    ImGui::Separator();
    if (ImGui::Button("Save###lib_prompt_save")) {
        DoSave();
        ApplyPrompted();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard###lib_prompt_discard")) {
        ApplyPrompted();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel###lib_prompt_cancel")) {
        ClosePrompt();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void RenderImportProblems() {
    if (g_import_open) ImGui::OpenPopup(kImportTitle);
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
    ImGui::SetNextWindowSizeConstraints(ImVec2(560.0F, 0.0F),
                                        ImVec2(560.0F, viewport->WorkSize.y - 80.0F));
    if (!ImGui::BeginPopupModal(kImportTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        g_import_open = false;
        return;
    }
    g_import_open = false;
    ImGui::TextWrapped("%s", g_import_path.c_str());
    ImGui::Separator();
    ImGui::TextWrapped("%s", g_import_message.c_str());
    ImGui::TextDisabled(g_import_blocked ? "Nothing was loaded. Fix the file and import it again."
                                         : "The document is loaded and saved under the free id.");
    ImGui::Separator();
    if (ImGui::Button("Copy report###lib_import_copy")) {
        ImGui::SetClipboardText((g_import_path + "\n" + g_import_message).c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Close###lib_import_close")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void RenderSaveAs() {
    if (g_saveas_open) ImGui::OpenPopup(kSaveAsTitle);
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5F, 0.5F));
    if (!ImGui::BeginPopupModal(kSaveAsTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        g_saveas_open = false;
        return;
    }
    g_saveas_open = false;
    ImGui::TextDisabled("Built-in ids are reserved, so this saves a user copy under a new id.");
    ImGui::SetNextItemWidth(320.0F);
    ImGui::InputText("###lib_saveas_id", g_saveas_id.data(), g_saveas_id.size());
    if (ImGui::Button("Save###lib_saveas_ok")) {
        const std::string id =
            Editor::UniqueId(Editor::Slug(g_saveas_id.data()),
                             TakenIds(Loaded() != nullptr ? Loaded()->build : std::string{}));
        Editor::Global().Apply([&id](Doc::Document& document) {
            document.id = id;
            return true;
        });
        const Doc::Document* document = Loaded();
        if (document != nullptr) SaveLoadedTo(*document);
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel###lib_saveas_cancel")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

}

std::vector<Doc::Problem> LoadedProblems() {
    const Doc::Document* document = Loaded();
    if (document == nullptr) return {};
    if (State().loaded_builtin) return Doc::Validate(*document);
    std::vector<std::string_view> builtin;
    for (const Doc::Entry* entry : State().registry.All()) {
        if (entry->builtin && entry->document.build == document->build)
            builtin.emplace_back(entry->document.id);
    }
    return Doc::Validate(*document, builtin);
}

void RequestPrompt(Action action, const std::string& select_id) {
    Library& state = State();
    state.prompted_action = action;
    state.pending_select = select_id;
    g_prompt_open = true;
}

void RequestImportProblems(const std::string& path, const std::string& message, bool blocked) {
    g_import_path = path;
    g_import_message = message;
    g_import_blocked = blocked;
    g_import_open = true;
}

void RequestSaveAs() {
    const Doc::Document* document = Loaded();
    const std::string suggested =
        document != nullptr ? Editor::CopyId(document->id, TakenIds(document->build)) : "";
    std::ranges::fill(g_saveas_id, '\0');
    std::copy_n(suggested.begin(), std::min(suggested.size(), g_saveas_id.size() - 1),
                g_saveas_id.begin());
    g_saveas_open = true;
}

void RunAction(Action action) {
    switch (action) {
    case Action::New:
        DoNew();
        break;
    case Action::Duplicate:
        DoDuplicate();
        break;
    case Action::Import:
        DoImport();
        break;
    case Action::Export:
        DoExport();
        break;
    case Action::Save:
        DoSave();
        break;
    case Action::SaveAs:
        RequestSaveAs();
        break;
    case Action::Revert:
        DoRevert(false);
        break;
    case Action::Reset:
        DoRevert(true);
        break;
    case Action::Properties:
        Timeline::RequestDocumentModal();
        break;
    case Action::Quit:
        App::Global().ConfirmClose();
        break;
    case Action::None:
    default:
        break;
    }
}

namespace {

void DrawFileActions(bool editable, bool built_in, const std::string& read_only) {
    ImGui::BeginDisabled(!editable);
    if (ImGui::Button("Save###lib_save")) RequestAction(Action::Save);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (!read_only.empty()) {
            ImGui::SetTooltip("%s. Duplicate it to get an editable copy for this build.",
                              read_only.c_str());
        } else {
            ImGui::SetTooltip(built_in
                                  ? "Built-in ids are reserved: this saves a user copy under a "
                                    "new id (Ctrl+S, Ctrl+Shift+S)."
                                  : "Write presets/<build>/<id>.json next to the renderer "
                                    "(Ctrl+S).");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Revert###lib_revert")) RequestAction(Action::Revert);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("%s", read_only.empty() ? "Throw the edits away and read the document "
                                                    "back from its file."
                                                  : read_only.c_str());
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!built_in);
    if (ImGui::Button("Reset###lib_reset")) RequestAction(Action::Reset);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Put a built-in document back to the one the renderer ships.");
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!editable);
    if (ImGui::Button("Properties...###lib_properties")) RequestAction(Action::Properties);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Name, notes, fps, length, render size, camera and lights.");
    }
}

void DrawRejected() {
    const std::vector<const Doc::Entry*> rejected = State().registry.Rejected();
    if (rejected.empty()) return;
    ImGui::TextDisabled("Files that did not load (%d)", (int)rejected.size());
    for (std::size_t i = 0; i < rejected.size(); i++) {
        const Doc::Entry& entry = *rejected[i];
        std::string message;
        for (const Doc::Problem& problem : entry.problems) {
            if (problem.severity != Doc::Severity::Error) continue;
            if (!message.empty()) message += "; ";
            message += problem.message;
        }
        const std::filesystem::path path(entry.path);
        std::string label = path.empty() ? entry.document.id : path.filename().string();
        label += ": ";
        label += message;
        label += "###lib_rejected_";
        label += std::to_string(i);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 0.45F, 0.45F, 1.0F));
        ImGui::Selectable(label.c_str());
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\n%s\nThe library cannot list it until the file is fixed.",
                              entry.path.c_str(), message.c_str());
        }
    }
}

}

void DrawActions() {
    const Doc::Document* document = Loaded();
    const bool loaded = document != nullptr;
    const std::string read_only = ReadOnlyReason();
    const bool editable = loaded && read_only.empty();
    const bool built_in = editable && IsBuiltInId(document->build, document->id);

    if (ImGui::Button("New###lib_new")) RequestAction(Action::New);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Start an empty document for this build (Ctrl+N).\nThe id is made "
                          "from the name when you press Done.");
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!loaded);
    if (ImGui::Button("Duplicate###lib_duplicate")) RequestAction(Action::Duplicate);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip(read_only.empty()
                              ? "Copy the loaded document under a free id, so a built-in can be "
                                "edited."
                              : "Copy it under a free id AND under this build, which is what makes "
                                "the copy editable and loadable here.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Import...###lib_import")) RequestAction(Action::Import);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Read a document from anywhere (Ctrl+O). A file that does not parse is "
                          "reported with its line and column and nothing is loaded.");
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!loaded);
    if (ImGui::Button("Export...###lib_export")) RequestAction(Action::Export);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Write the loaded document as canonical JSON anywhere on disk.");
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    DrawFileActions(editable, built_in, read_only);
}

void DrawProblems() {
    DrawRejected();
    const std::vector<Doc::Problem> problems = LoadedProblems();
    if (problems.empty()) {
        if (State().registry.Rejected().empty()) ImGui::TextDisabled("no validation problems");
        return;
    }
    ImGui::TextDisabled("Validation (%d)", (int)problems.size());
    for (std::size_t i = 0; i < problems.size(); i++) {
        const Doc::Problem& problem = problems[i];
        const bool error = problem.severity == Doc::Severity::Error;
        ImGui::PushStyleColor(ImGuiCol_Text, error ? ImVec4(1.0F, 0.45F, 0.45F, 1.0F)
                                                   : ImVec4(1.0F, 0.85F, 0.3F, 1.0F));
        const std::string label =
            problem.path + ": " + problem.message + "###lib_problem_" + std::to_string(i);
        if (ImGui::Selectable(label.c_str())) {
            Editor::State& editor = Editor::Global();
            if (editor.Loaded() && Editor::FindClip(editor.Document(), problem.path).Valid())
                editor.Select(problem.path);
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\n%s\nClick to select the clip it names.", problem.path.c_str(),
                              problem.message.c_str());
        }
    }
}

void RenderModals() {
    PumpCloseRequest();
    RenderPrompt();
    RenderImportProblems();
    RenderSaveAs();
}

}
