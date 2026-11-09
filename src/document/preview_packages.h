#pragma once

#include "formats/ifs_archive.h"

namespace Document {

struct PreviewPackages {
    Ifs::Archive textures;
    Ifs::Archive content;
};

[[nodiscard]] PreviewPackages SplitPreviewPackages(const Ifs::Archive& archive);

[[nodiscard]] bool HasTextures(const PreviewPackages& packages);

}
