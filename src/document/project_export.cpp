#include "document/project_export.h"

#include "document/atlas.h"
#include "document/atlas_write.h"
#include "document/authored.h"
#include "document/document.h"
#include "document/project.h"
#include "document/project_drift.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace Document {

namespace {

std::vector<std::string> AnimationsOwned(const Project& project) {
    std::vector<std::string> paths;
    for (const AuthoredDepth& depth : project.content) {
        if (std::ranges::find(paths, depth.animation) == paths.end())
            paths.push_back(depth.animation);
    }
    std::ranges::sort(paths);
    return paths;
}

Support::Expected<void, std::string> ExportImages(File& file, const Project& project,
                                                  const ImageLoader& load) {
    if (project.images.empty()) return {};
    std::vector<SourceImage> sorted = project.images;
    std::ranges::sort(sorted, {}, &SourceImage::name);

    std::vector<AtlasCell> cells;
    std::vector<LoadedImage> guarded;
    for (const SourceImage& image : sorted) {
        auto loaded = load(image.file);
        if (!loaded) return Support::Unexpected(image.name + " cannot be read: " + loaded.error());
        LoadedImage ringed = WithGuardRing(*loaded);
        cells.push_back(
            AtlasCell{.name = image.name, .width = ringed.width, .height = ringed.height});
        guarded.push_back(std::move(ringed));
    }

    auto atlas = PackAtlas(cells);
    if (!atlas) return Support::Unexpected(atlas.error());

    std::vector<LoadedImage> ordered;
    ordered.reserve(atlas->images.size());
    for (const AtlasPlacement& placed : atlas->images) {
        const auto at = std::ranges::find(cells, placed.name, &AtlasCell::name);
        ordered.push_back(guarded[static_cast<std::size_t>(at - cells.begin())]);
    }
    return file.WriteAtlas(kProjectAtlasName, *atlas, ordered);
}

}

Support::Expected<void, std::string> ExportProject(File& file, Project& project,
                                                   const ImageLoader& load) {
    auto images = ExportImages(file, project, load);
    if (!images) return Support::Unexpected(images.error());

    for (const std::string& path : AnimationsOwned(project)) {
        auto animation = file.ReadAnimation(path);
        if (!animation) return Support::Unexpected(animation.error());

        std::vector<const AuthoredDepth*> owned;
        for (const AuthoredDepth& depth : project.content) {
            if (depth.animation == path) owned.push_back(&depth);
        }
        std::ranges::sort(owned, {}, [](const AuthoredDepth* depth) {
            return std::pair{depth->depth, depth->first_frame};
        });

        for (const AuthoredDepth* depth : owned) {
            auto baked = BakedFor(*animation, *depth);
            if (!baked) {
                return Support::Unexpected("depth " + std::to_string(depth->depth) + " of " + path +
                                           " cannot be written: " + baked.error());
            }
            auto written = WriteAuthored(*animation, *depth, *baked);
            if (!written) {
                return Support::Unexpected("depth " + std::to_string(depth->depth) + " of " + path +
                                           " cannot be written: " + written.error());
            }
        }

        auto stored = file.WriteAnimation(path, *animation);
        if (!stored) return Support::Unexpected(stored.error());
    }
    project.exported = RecordExported(file, project);
    return {};
}

}
