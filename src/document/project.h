#pragma once

#include "document/authored.h"
#include "support/expected.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

inline constexpr int kProjectFormat = 1;
inline constexpr std::string_view kProjectManifestName = "project.json";
inline constexpr std::string_view kProjectSourceDirectory = "sources";
inline constexpr std::string_view kProjectAtlasName = "project";

struct SourceImage {
    std::string name;
    std::string file;

    friend bool operator==(const SourceImage&, const SourceImage&) = default;
};

struct Project {
    std::string build;
    std::string ifs_path;
    std::vector<AuthoredDepth> content;
    std::vector<SourceImage> images;

    friend bool operator==(const Project&, const Project&) = default;
};

[[nodiscard]] Support::Expected<Project, std::string>
ReadProject(std::span<const uint8_t> manifest);

[[nodiscard]] std::vector<uint8_t> WriteProject(const Project& project);

[[nodiscard]] std::string StoredIfsPath(std::string_view folder, std::string_view ifs);

[[nodiscard]] std::string ResolvedIfsPath(std::string_view folder, const Project& project);

[[nodiscard]] std::string ProjectManifestPath(std::string_view folder);

[[nodiscard]] std::string ProjectSourcePath(std::string_view folder, std::string_view file);

}
