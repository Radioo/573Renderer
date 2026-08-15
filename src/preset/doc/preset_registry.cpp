#include "preset/doc/preset_registry.h"

#include "preset/defaults/defaults.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_json.h"
#include "preset/doc/preset_validate.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <windows.h>

namespace Preset::Doc {

namespace {

bool HasError(const std::vector<Problem>& problems) {
    return std::ranges::any_of(
        problems, [](const Problem& problem) { return problem.severity == Severity::Error; });
}

std::vector<std::filesystem::path> UserFiles(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> files;
    std::error_code code;
    if (!std::filesystem::is_directory(root, code)) return files;
    for (const std::filesystem::directory_entry& build :
         std::filesystem::directory_iterator(root, code)) {
        if (!build.is_directory()) continue;
        for (const std::filesystem::directory_entry& file :
             std::filesystem::directory_iterator(build.path(), code)) {
            if (file.is_regular_file() && file.path().extension() == ".json")
                files.push_back(file.path());
        }
    }
    return files;
}

Problem ParseProblem(const ParseError& error) {
    std::string message = "parse error at line " + std::to_string(error.line) + ", column " +
                          std::to_string(error.column) + ": " + error.message;
    return Problem{.severity = Severity::Error, .path = error.path, .message = std::move(message)};
}

const Entry* Claiming(const std::vector<Entry>& known, const Document& document) {
    for (const Entry& entry : known) {
        if (entry.document.build == document.build && entry.document.id == document.id)
            return &entry;
    }
    return nullptr;
}

std::vector<std::string_view> BuiltInIds(const std::vector<Entry>& known, std::string_view build) {
    std::vector<std::string_view> ids;
    for (const Entry& entry : known) {
        if (entry.builtin && entry.document.build == build) ids.push_back(entry.document.id);
    }
    return ids;
}

const Entry* Examine(Entry& entry, const std::vector<Entry>& known,
                     const std::filesystem::path& path) {
    const Entry* taken = Claiming(known, entry.document);
    entry.problems = Validate(entry.document, BuiltInIds(known, entry.document.build));
    if (taken != nullptr && !taken->builtin) {
        entry.problems.push_back(Problem{.severity = Severity::Error,
                                         .path = entry.document.id,
                                         .message = "id \"" + entry.document.id +
                                                    "\" is already used by " + taken->path});
    }
    const std::string directory = path.parent_path().filename().string();
    if (directory != entry.document.build) {
        entry.problems.push_back(Problem{.severity = Severity::Warning,
                                         .path = entry.document.id,
                                         .message = "the file sits under presets/" + directory +
                                                    " but the document's build is \"" +
                                                    entry.document.build + "\""});
    }
    return taken;
}

}

std::filesystem::path UserRoot() {
    std::array<char, MAX_PATH> exe{};
    GetModuleFileNameA(nullptr, exe.data(), (DWORD)exe.size());
    return std::filesystem::path(exe.data()).parent_path() / "presets";
}

Loaded LoadFile(const std::filesystem::path& path) {
    const std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
        return Support::Unexpected(
            ParseError{.path = path.string(), .message = "cannot read the file"});
    }
    std::ostringstream text;
    text << file.rdbuf();
    return Load(text.str());
}

void Registry::Load(const std::filesystem::path& user_root, const ScanProgressFn& progress) {
    entries_.clear();
    rejected_.clear();
    for (Document& document : BuiltIns()) {
        entries_.push_back(Entry{.document = std::move(document), .builtin = true});
    }

    const std::vector<std::filesystem::path> files = UserFiles(user_root);
    ScanStatus status;
    status.total = (int)files.size();
    for (const std::filesystem::path& path : files) {
        status.current = path.string();
        status.done++;
        if (progress) progress(status);

        Entry entry;
        entry.path = path.string();
        const Loaded loaded = LoadFile(path);
        if (!loaded.has_value()) {
            entry.problems.push_back(ParseProblem(loaded.error()));
            rejected_.push_back(std::move(entry));
            continue;
        }
        entry.document = *loaded;
        if (Examine(entry, entries_, path) != nullptr) {
            rejected_.push_back(std::move(entry));
            continue;
        }
        entries_.push_back(std::move(entry));
    }
    if (progress && files.empty()) progress(status);
}

std::vector<const Entry*> Registry::ForBuild(std::string_view build) const {
    std::vector<const Entry*> matched;
    for (const Entry& entry : entries_) {
        if (entry.document.build == build) matched.push_back(&entry);
    }
    return matched;
}

const Entry* Registry::Find(std::string_view build, std::string_view id) const {
    for (const Entry& entry : entries_) {
        if (entry.document.build == build && entry.document.id == id) return &entry;
    }
    return nullptr;
}

std::vector<const Entry*> Registry::Problems() const {
    std::vector<const Entry*> listed;
    for (const Entry& entry : entries_) {
        if (HasError(entry.problems)) listed.push_back(&entry);
    }
    for (const Entry& entry : rejected_)
        listed.push_back(&entry);
    return listed;
}

}
