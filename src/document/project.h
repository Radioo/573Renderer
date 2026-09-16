#pragma once

#include "support/expected.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

inline constexpr int kProjectFormat = 1;
inline constexpr std::string_view kProjectManifestName = "project.json";

struct Project {
    std::string build;
    std::string ifs_path;

    friend bool operator==(const Project&, const Project&) = default;
};

[[nodiscard]] Support::Expected<Project, std::string>
ReadProject(std::span<const uint8_t> manifest);

[[nodiscard]] std::vector<uint8_t> WriteProject(const Project& project);

[[nodiscard]] std::string StoredIfsPath(std::string_view folder, std::string_view ifs);

[[nodiscard]] std::string ResolvedIfsPath(std::string_view folder, const Project& project);

[[nodiscard]] std::string ProjectManifestPath(std::string_view folder);

}
