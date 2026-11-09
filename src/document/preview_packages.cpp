#include "document/preview_packages.h"

#include "formats/ifs_archive.h"

#include <algorithm>
#include <string_view>

namespace Document {

namespace {

constexpr std::string_view kTextureDirectory = "tex";

bool IsTextures(const Ifs::Entry& entry) {
    return entry.kind == Ifs::EntryKind::Directory && entry.name == kTextureDirectory;
}

bool IsShared(const Ifs::Entry& entry) {
    return entry.kind != Ifs::EntryKind::Directory;
}

}

PreviewPackages SplitPreviewPackages(const Ifs::Archive& archive) {
    PreviewPackages split{.textures = archive, .content = archive};
    split.textures.entries.clear();
    split.content.entries.clear();
    for (const Ifs::Entry& entry : archive.entries) {
        if (IsShared(entry) || IsTextures(entry)) split.textures.entries.push_back(entry);
        if (!IsTextures(entry)) split.content.entries.push_back(entry);
    }
    return split;
}

bool HasTextures(const PreviewPackages& packages) {
    return std::ranges::any_of(packages.textures.entries, IsTextures);
}

}
