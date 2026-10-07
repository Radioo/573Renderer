#pragma once

#include "document/atlas_write.h"
#include "document/document.h"
#include "document/project.h"
#include "support/expected.h"

#include <functional>
#include <string>

namespace Document {

using ImageLoader =
    std::function<Support::Expected<LoadedImage, std::string>(const std::string& file)>;

[[nodiscard]] Support::Expected<void, std::string> ExportProject(File& file, Project& project,
                                                                 const ImageLoader& load);

}
