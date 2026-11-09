#include <catch2/catch_test_macros.hpp>

#include "sample_package.h"

#include "document/preview_packages.h"
#include "formats/ifs_archive.h"

#include <string>
#include <vector>

namespace {

std::vector<std::string> Names(const Ifs::Archive& archive) {
    std::vector<std::string> names;
    names.reserve(archive.entries.size());
    for (const Ifs::Entry& entry : archive.entries)
        names.push_back(entry.name);
    return names;
}

}

TEST_CASE("A preview splits a package into its textures and the rest") {
    Ifs::Archive archive = SamplePackage::SampleArchive();
    archive.entries.push_back(SamplePackage::File("version_Exml", {1, 2}));
    archive.entries.push_back(SamplePackage::Directory("geo", {}));
    archive.time = 1234;
    const Document::PreviewPackages split = Document::SplitPreviewPackages(archive);
    CHECK(Names(split.textures) ==
          std::vector<std::string>{"magic", "tex", "_info_", "version_Exml"});
    CHECK(Names(split.content) ==
          std::vector<std::string>{"magic", "afp", "_info_", "version_Exml", "geo"});
    CHECK(split.textures.time == 1234);
    CHECK(split.content.time == 1234);
    CHECK(Ifs::Write(split.textures).has_value());
    CHECK(Ifs::Write(split.content).has_value());
}

TEST_CASE("A package with no textures has an empty texture half") {
    Ifs::Archive archive = SamplePackage::SampleArchive();
    std::erase_if(archive.entries, [](const Ifs::Entry& entry) { return entry.name == "tex"; });
    const Document::PreviewPackages split = Document::SplitPreviewPackages(archive);
    CHECK_FALSE(Document::HasTextures(split));
    const Ifs::Archive with = SamplePackage::SampleArchive();
    CHECK(Document::HasTextures(Document::SplitPreviewPackages(with)));
}
