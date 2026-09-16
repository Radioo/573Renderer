#include "document/project_export.h"

#include "document/authored.h"
#include "document/document.h"
#include "document/project.h"
#include "support/expected.h"

#include <algorithm>
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

}

Support::Expected<void, std::string> ExportProject(File& file, const Project& project) {
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
    return {};
}

}
