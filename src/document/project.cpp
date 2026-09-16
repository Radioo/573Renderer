#include "document/project.h"

#include "support/expected.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Document {

namespace {

using Json = nlohmann::ordered_json;

constexpr std::string_view kFormatKey = "format";
constexpr std::string_view kBuildKey = "build";
constexpr std::string_view kIfsKey = "ifs";
constexpr int kIndent = 2;

Support::Expected<std::string, std::string> Text(const Json& object, std::string_view key) {
    const auto found = object.find(std::string(key));
    if (found == object.end())
        return Support::Unexpected("the project manifest has no \"" + std::string(key) + "\"");
    if (!found.value().is_string())
        return Support::Unexpected("the project's \"" + std::string(key) + "\" is not text");
    return found.value().get<std::string>();
}

std::filesystem::path Generic(std::string_view text) {
    return std::filesystem::path(std::string(text)).lexically_normal();
}

}

Support::Expected<Project, std::string> ReadProject(std::span<const uint8_t> manifest) {
    Json parsed;
    try {
        parsed = Json::parse(manifest.begin(), manifest.end());
    } catch (const std::exception& bad) {
        return Support::Unexpected("the project manifest is not readable: " +
                                   std::string(bad.what()));
    }
    if (!parsed.is_object())
        return Support::Unexpected(std::string("the project manifest is not an object"));

    const auto format = parsed.find(std::string(kFormatKey));
    if (format == parsed.end() || !format.value().is_number_integer())
        return Support::Unexpected(std::string("the project manifest has no format number"));
    const int written = format.value().get<int>();
    if (written != kProjectFormat) {
        return Support::Unexpected("this project was written in format " + std::to_string(written) +
                                   " and the editor reads format " +
                                   std::to_string(kProjectFormat));
    }

    auto build = Text(parsed, kBuildKey);
    if (!build) return Support::Unexpected(build.error());
    auto ifs = Text(parsed, kIfsKey);
    if (!ifs) return Support::Unexpected(ifs.error());
    if (ifs->empty()) return Support::Unexpected(std::string("the project names no IFS"));
    return Project{.build = std::move(*build), .ifs_path = std::move(*ifs)};
}

std::vector<uint8_t> WriteProject(const Project& project) {
    Json out;
    out[std::string(kFormatKey)] = kProjectFormat;
    out[std::string(kBuildKey)] = project.build;
    out[std::string(kIfsKey)] = project.ifs_path;
    std::string text = out.dump(kIndent);
    text += '\n';
    return {text.begin(), text.end()};
}

std::string StoredIfsPath(std::string_view folder, std::string_view ifs) {
    const std::filesystem::path here = Generic(folder);
    const std::filesystem::path target = Generic(ifs);
    const std::filesystem::path relative = target.lexically_relative(here);
    if (relative.empty()) return target.generic_string();
    return relative.generic_string();
}

std::string ResolvedIfsPath(std::string_view folder, const Project& project) {
    const std::filesystem::path stored = Generic(project.ifs_path);
    if (stored.is_absolute()) return stored.generic_string();
    return (Generic(folder) / stored).lexically_normal().generic_string();
}

std::string ProjectManifestPath(std::string_view folder) {
    return (Generic(folder) / kProjectManifestName).generic_string();
}

}
