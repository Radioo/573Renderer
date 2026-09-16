#include "document/project_drift.h"

#include "document/authored.h"
#include "document/document.h"
#include "document/entries.h"
#include "document/entry_edit.h"
#include "document/project.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Document {

namespace {

constexpr std::string_view kTextureDirectory = "tex";
constexpr std::string_view kTextureList = "texturelist.xml";

void Remember(std::vector<std::string>& paths, std::string path) {
    if (path.empty()) return;
    if (std::ranges::find(paths, path) != paths.end()) return;
    paths.push_back(std::move(path));
}

}

std::vector<std::string> ExportedPaths(const File& file, const Project& project) {
    std::vector<std::string> paths;
    for (const AuthoredDepth& depth : project.content) {
        Remember(paths, depth.animation);
        Remember(paths, ScriptPath(depth.animation));
    }
    if (!project.images.empty()) {
        Remember(paths, JoinPath(kTextureDirectory, kTextureList));
        for (const SourceImage& image : project.images) {
            auto stored = StoredName(kTextureDirectory, image.name);
            if (stored) Remember(paths, JoinPath(kTextureDirectory, *stored));
        }
    }
    std::ranges::sort(paths);
    std::erase_if(paths,
                  [&file](const std::string& path) { return !file.EntryDigest(path).has_value(); });
    return paths;
}

std::vector<ExportedEntry> RecordExported(const File& file, const Project& project) {
    std::vector<ExportedEntry> out;
    for (const std::string& path : ExportedPaths(file, project)) {
        const std::optional<std::string> digest = file.EntryDigest(path);
        if (!digest) continue;
        out.push_back(ExportedEntry{.path = path, .digest = *digest});
    }
    return out;
}

std::vector<DriftedEntry> ProjectDrift(const File& file, const Project& project) {
    std::vector<DriftedEntry> out;
    for (const ExportedEntry& entry : project.exported) {
        const std::optional<std::string> digest = file.EntryDigest(entry.path);
        if (!digest) {
            out.push_back(DriftedEntry{.path = entry.path, .kind = DriftKind::Missing});
            continue;
        }
        if (*digest != entry.digest)
            out.push_back(DriftedEntry{.path = entry.path, .kind = DriftKind::Changed});
    }
    return out;
}

void KeepIfsVersion(Project& project, std::string_view path) {
    const std::string wanted(path);
    std::erase_if(project.exported,
                  [&wanted](const ExportedEntry& entry) { return entry.path == wanted; });
    std::erase_if(project.content,
                  [&wanted](const AuthoredDepth& depth) { return depth.animation == wanted; });
}

}
