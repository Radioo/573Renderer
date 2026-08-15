#include "preset/preset_tools.h"

#include "preset/defaults/defaults.h"
#include "preset/doc/preset_document.h"
#include "preset/doc/preset_json.h"
#include "preset/doc/preset_registry.h"
#include "preset/doc/preset_validate.h"
#include "support/log.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <vector>

namespace PresetTools {

namespace {

namespace Doc = Preset::Doc;

bool WriteText(const std::filesystem::path& path, const std::string& text) {
    std::error_code code;
    std::filesystem::create_directories(path.parent_path(), code);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.good()) {
        LOG("PresetTools", "cannot write %s", path.string().c_str());
        return false;
    }
    file.write(text.data(), (std::streamsize)text.size());
    return file.good();
}

}

int DumpDefaults(const std::string& out_dir) {
    if (out_dir.empty()) {
        LOG("PresetTools", "--preset-dump-defaults wants an output directory to write into");
        return 2;
    }
    const std::vector<Doc::Document> documents = Doc::BuiltIns();
    if (documents.empty()) {
        LOG("PresetTools", "no built-in preset documents to dump");
        return 2;
    }
    int written = 0;
    for (const Doc::Document& document : documents) {
        const std::filesystem::path path =
            std::filesystem::path(out_dir) / document.build / (document.id + ".json");
        LOG("PresetTools", "dumping %d/%d: %s", written + 1, (int)documents.size(),
            path.string().c_str());
        if (!WriteText(path, Doc::Save(document))) return 2;
        written++;
    }
    LOG("PresetTools", "wrote %d built-in document(s) under %s", written, out_dir.c_str());
    return 0;
}

int ExportJson(const std::string& build, const std::string& preset_id,
               const std::string& out_path) {
    Doc::Registry registry;
    registry.Load(Doc::UserRoot(), [](const Doc::ScanStatus& status) {
        LOG("PresetTools", "scanning user presets %d/%d: %s", status.done, status.total,
            status.current.c_str());
    });
    const Doc::Entry* entry = registry.Find(build, preset_id);
    if (entry == nullptr) {
        LOG("PresetTools", "build '%s' has no preset '%s'", build.c_str(), preset_id.c_str());
        return 2;
    }
    if (!WriteText(out_path, Doc::Save(entry->document))) return 2;
    LOG("PresetTools", "wrote %s -> %s", preset_id.c_str(), out_path.c_str());
    return 0;
}

int Validate(const std::string& path) {
    const Doc::Loaded loaded = Doc::LoadFile(path);
    if (!loaded.has_value()) {
        LOG("PresetTools", "%s: %s (line %d, column %d)", path.c_str(),
            loaded.error().message.c_str(), loaded.error().line, loaded.error().column);
        return 2;
    }
    int errors = 0;
    for (const Doc::Problem& problem : Doc::Validate(*loaded)) {
        const bool fatal = problem.severity == Doc::Severity::Error;
        errors += fatal ? 1 : 0;
        LOG("PresetTools", "%s %s: %s", fatal ? "error" : "warning", problem.path.c_str(),
            problem.message.c_str());
    }
    LOG("PresetTools", "%s: %s, %d error(s)", path.c_str(), loaded->id.c_str(), errors);
    return (errors > 0) ? 1 : 0;
}

}
