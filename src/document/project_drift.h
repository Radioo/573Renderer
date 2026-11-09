#pragma once

#include "document/document.h"
#include "document/project.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

enum class DriftKind : uint8_t { Changed, Missing };

struct DriftedEntry {
    std::string path;
    DriftKind kind = DriftKind::Changed;

    friend bool operator==(const DriftedEntry&, const DriftedEntry&) = default;
};

[[nodiscard]] std::vector<std::string> ExportedPaths(const File& file, const Project& project);

[[nodiscard]] std::vector<ExportedEntry> RecordExported(const File& file, const Project& project);

[[nodiscard]] std::vector<DriftedEntry> ProjectDrift(const File& file, const Project& project);

[[nodiscard]] std::size_t AwaitingExport(const File& file, const Project& project);

void KeepIfsVersion(Project& project, std::string_view path);

}
