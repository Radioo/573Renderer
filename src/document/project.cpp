#include "document/project.h"
#include "document/project_content.h"

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
constexpr std::string_view kContentKey = "owns";
constexpr std::string_view kImagesKey = "images";
constexpr int kIndent = 2;

Support::Expected<std::string, std::string> Text(const Json& object, std::string_view key) {
    const auto found = object.find(std::string(key));
    if (found == object.end())
        return Support::Unexpected("the project manifest has no \"" + std::string(key) + "\"");
    if (!found.value().is_string())
        return Support::Unexpected("the project's \"" + std::string(key) + "\" is not text");
    return found.value().get<std::string>();
}

Support::Expected<std::vector<SourceImage>, std::string> ReadImages(const Json& parsed,
                                                                    std::string_view key) {
    std::vector<SourceImage> out;
    const auto images = parsed.find(std::string(key));
    if (images == parsed.end()) return out;
    if (!images.value().is_array())
        return Support::Unexpected(std::string("the project's images are not a list"));
    for (const Json& image : images.value()) {
        const auto name = image.is_object() ? image.find("name") : image.end();
        const auto file = image.is_object() ? image.find("file") : image.end();
        if (name == image.end() || !name.value().is_string() || file == image.end() ||
            !file.value().is_string()) {
            return Support::Unexpected(std::string("an image names no source file"));
        }
        out.push_back(SourceImage{.name = name.value().get<std::string>(),
                                  .file = file.value().get<std::string>()});
    }
    return out;
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
    Project project{
        .build = std::move(*build), .ifs_path = std::move(*ifs), .content = {}, .images = {}};
    const auto content = parsed.find(std::string(kContentKey));
    if (content != parsed.end()) {
        auto read = ReadContent(content.value());
        if (!read) return Support::Unexpected(read.error());
        project.content = std::move(*read);
    }
    auto pictures = ReadImages(parsed, kImagesKey);
    if (!pictures) return Support::Unexpected(pictures.error());
    project.images = std::move(*pictures);
    return project;
}

std::vector<uint8_t> WriteProject(const Project& project) {
    Json out;
    out[std::string(kFormatKey)] = kProjectFormat;
    out[std::string(kBuildKey)] = project.build;
    out[std::string(kIfsKey)] = project.ifs_path;
    out[std::string(kContentKey)] = WriteContent(project.content);
    Json images = Json::array();
    for (const SourceImage& image : project.images) {
        Json one;
        one["name"] = image.name;
        one["file"] = image.file;
        images.push_back(std::move(one));
    }
    out[std::string(kImagesKey)] = std::move(images);
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

std::string ProjectSourcePath(std::string_view folder, std::string_view file) {
    return (Generic(folder) / Generic(file)).generic_string();
}

}
