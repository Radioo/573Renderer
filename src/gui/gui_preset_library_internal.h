#pragma once

#include "editor/library_model.h"
#include "gui_preset_library.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_json.h"
#include "preset/doc/preset_registry.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Panels::PresetLibrary {

struct Library {
    Preset::Doc::Registry registry;
    std::string root_override;
    std::string scanned_dir;
    std::string build_id;
    std::string build_name;
    std::string selected;
    std::string pending_select;
    Action pending_action = Action::None;
    Action prompted_action = Action::None;
    bool new_document_pending = false;
    bool loaded_builtin = false;
    bool scanned = false;
};

Library& State();

std::filesystem::path Root();

std::vector<Editor::LibraryEntry> Entries();

std::vector<std::string> TakenIds(const std::string& build);

void Rescan(bool report_progress);

void OpenDocument(const Preset::Doc::Document& document, bool from_builtin);

void SelectEntry(const std::string& id);

void RunAction(Action action);

void RequestPrompt(Action action, const std::string& select_id);

void RequestImportProblems(const std::string& path, const std::string& message, bool blocked);

void RequestSaveAs();

bool IsBuiltInId(const std::string& build, const std::string& id);

bool ForeignBuild(const Preset::Doc::Document& document);

std::string ReadOnlyReason();

const Preset::Doc::Entry* FindEntry(const std::string& id);

std::vector<Preset::Doc::Problem> LoadedProblems();

void DrawProblems();

void DrawActions();

}
