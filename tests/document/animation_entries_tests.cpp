#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sample_package.h"

#include "document/animation_strings.h"
#include "document/document.h"
#include "document/entry_edit.h"
#include "document/image_shape.h"
#include "document/animation_entries.h"
#include "document/place_image.h"
#include "formats/afp_animation.h"
#include "formats/binary_xml.h"
#include "formats/ifs_archive.h"
#include "formats/ifs_names.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using SamplePackage::HashPath;

Ifs::Archive Package() {
    Ifs::Archive archive = SamplePackage::SampleArchive();
    for (Ifs::Entry& directory : archive.entries) {
        if (directory.name != "afp") continue;
        for (Ifs::Entry& entry : directory.children) {
            if (entry.name != "afplist_Exml") continue;
            auto list = BinaryXml::Read(entry.bytes);
            REQUIRE(list.has_value());
            if (!list) continue;
            list->root.children.at(0).attributes.at(0).value.push_back(0);
            entry.bytes = *BinaryXml::Write(*list);
        }
    }
    return archive;
}

std::vector<BinaryXml::Node> Listings(const Ifs::Archive& archive) {
    std::vector<BinaryXml::Node> listings;
    for (const Ifs::Entry& directory : archive.entries) {
        if (directory.name != "afp") continue;
        for (const Ifs::Entry& entry : directory.children) {
            if (entry.name != "afplist_Exml") continue;
            const auto list = BinaryXml::Read(entry.bytes);
            REQUIRE(list.has_value());
            if (!list) continue;
            listings.insert(listings.end(), list->root.children.begin(), list->root.children.end());
        }
    }
    return listings;
}

std::vector<std::vector<uint8_t>> ListedNames(const Ifs::Archive& archive) {
    std::vector<std::vector<uint8_t>> names;
    for (const BinaryXml::Node& listed : Listings(archive)) {
        CHECK(listed.name == "afp");
        names.push_back(listed.attributes.at(0).value);
    }
    return names;
}

std::vector<uint8_t> Bytes(const std::string& text) {
    std::vector<uint8_t> bytes(text.begin(), text.end());
    bytes.push_back(0);
    return bytes;
}

std::vector<std::string> StoredNames(const Ifs::Archive& archive, const std::string& directory) {
    std::vector<std::string> names;
    for (const Ifs::Entry& entry : archive.entries) {
        if (entry.name != directory) continue;
        for (const Ifs::Entry& child : entry.children) {
            if (child.kind == Ifs::EntryKind::File) names.push_back(child.name);
            if (child.kind != Ifs::EntryKind::Directory) continue;
            for (const Ifs::Entry& nested : child.children)
                names.push_back(child.name + "/" + nested.name);
        }
    }
    return names;
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

}

TEST_CASE("A new animation takes another's header and starts with empty frames") {
    Ifs::Archive archive = Package();
    const auto path = Document::AddAnimation(archive, "fresh", "afp/" + HashPath("intro"), 30);
    INFO(Error(path));
    REQUIRE(path.has_value());
    if (!path) return;
    CHECK(*path == "afp/" + HashPath("fresh"));
    CHECK(ListedNames(archive) ==
          std::vector<std::vector<uint8_t>>{Bytes("intro"), Bytes("fresh")});
    CHECK(Listings(archive).back().children.empty());

    const auto encoded = Ifs::Write(archive);
    REQUIRE(encoded.has_value());
    if (!encoded) return;
    auto file = Document::File::Open(*encoded);
    REQUIRE(file.has_value());
    if (!file) return;
    const auto made = file->ReadAnimation(*path);
    REQUIRE(made.has_value());
    if (!made) return;
    const AfpAnimation::Animation like = SamplePackage::SampleAnimation();
    CHECK(Document::StringText(*made, made->name) == "fresh");
    CHECK(made->strings == std::vector<std::string>{"", "fresh"});
    CHECK(made->root.frames.size() == 30);
    CHECK(made->root.tags.empty());
    CHECK(made->root.labels.empty());
    CHECK(made->exports.empty());
    CHECK(made->container_version == like.container_version);
    CHECK(made->magic == like.magic);
    CHECK(made->data_version == like.data_version);
    CHECK(made->flags == like.flags);
    CHECK(made->rect == like.rect);
    CHECK(made->fps == like.fps);
    CHECK(made->stored_form == like.stored_form);

    REQUIRE(file->AddImage("added", 4, 3, std::vector<uint8_t>(48, 0x40)).has_value());
    const auto placed = Document::PlaceImage(
        *file, *path, "added",
        Document::DepthSpan{.clip = {}, .depth = 1, .first_frame = 0, .last_frame = 29});
    INFO(Error(placed));
    CHECK(placed.has_value());
}

TEST_CASE("A new animation keeps what its template imports") {
    Ifs::Archive archive = Package();
    AfpAnimation::Animation like = SamplePackage::SampleAnimation();
    like.strings = {"", "loop", "__Packages.aeplib", "aeplib"};
    like.imports = {AfpAnimation::Import{
        .movie = 2, .assets = {AfpAnimation::ImportedAsset{.tag = 2, .name = 3}}}};
    like.import_initializers = AfpAnimation::ImportInitializers{
        .leading_word = 0, .entries = {AfpAnimation::ImportInitializer{.tag = 2, .frame = 0}}};
    like.flags = 0xC7;
    const auto stored = AfpAnimation::WriteStored(like);
    REQUIRE(stored.has_value());
    if (!stored) return;
    REQUIRE(Document::ReplaceEntry(archive, "afp/" + HashPath("intro"), stored->data).has_value());
    REQUIRE(Document::ReplaceEntry(archive, "afp/bsi/" + HashPath("intro"), stored->script)
                .has_value());

    const auto path = Document::AddAnimation(archive, "fresh", "afp/" + HashPath("intro"), 2);
    INFO(Error(path));
    REQUIRE(path.has_value());
    if (!path) return;
    const auto encoded = Ifs::Write(archive);
    REQUIRE(encoded.has_value());
    if (!encoded) return;
    auto file = Document::File::Open(*encoded);
    REQUIRE(file.has_value());
    if (!file) return;
    const auto made = file->ReadAnimation(*path);
    REQUIRE(made.has_value());
    if (!made) return;
    const auto template_read = file->ReadAnimation("afp/" + HashPath("intro"));
    REQUIRE(template_read.has_value());
    if (!template_read) return;
    CHECK(made->flags == template_read->flags);
    REQUIRE(made->imports.size() == 1);
    CHECK(Document::StringText(*made, made->imports[0].movie) == "__Packages.aeplib");
    CHECK(Document::StringText(*made, made->imports[0].assets.at(0).name) == "aeplib");
    CHECK(made->import_initializers == like.import_initializers);
    CHECK(std::ranges::find(made->strings, "loop") == made->strings.end());
}

TEST_CASE("A new animation needs a free and loadable name, a frame and a template") {
    const std::string like = "afp/" + HashPath("intro");
    Ifs::Archive archive = Package();
    const auto before = Ifs::Write(archive);
    REQUIRE(before.has_value());
    CHECK_FALSE(Document::AddAnimation(archive, "intro", like, 1).has_value());
    CHECK_FALSE(Document::AddAnimation(archive, "", like, 1).has_value());
    CHECK_FALSE(Document::AddAnimation(archive, std::string(53, 'a'), like, 1).has_value());
    CHECK_FALSE(Document::AddAnimation(archive, "a/b", like, 1).has_value());
    CHECK_FALSE(Document::AddAnimation(archive, "tab\there", like, 1).has_value());
    CHECK_FALSE(Document::AddAnimation(archive, "caf\xc3\xa9", like, 1).has_value());
    CHECK_FALSE(Document::AddAnimation(archive, "fresh", like, 0).has_value());
    CHECK_FALSE(
        Document::AddAnimation(archive, "fresh", "afp/" + HashPath("nothing"), 1).has_value());
    const auto after = Ifs::Write(archive);
    REQUIRE(after.has_value());
    CHECK(*after == *before);
    CHECK(Document::AddAnimation(archive, std::string(52, 'a'), like, 1).has_value());
}

TEST_CASE("Removing an animation takes its data, byte order, listing and shapes") {
    Ifs::Archive archive = Package();
    const std::string intro = "afp/" + HashPath("intro");
    const auto fresh = Document::AddAnimation(archive, "fresh", intro, 4);
    REQUIRE(fresh.has_value());
    if (!fresh) return;
    REQUIRE(Document::AddImage(archive, "added", 4, 3, std::vector<uint8_t>(48, 0x40)).has_value());
    const auto shape = Document::AddImageShape(archive, *fresh, "added");
    INFO(Error(shape));
    REQUIRE(shape.has_value());
    const auto kept = Document::AddAnimation(archive, "kept", intro, 4);
    REQUIRE(kept.has_value());
    if (!kept) return;
    const auto other = Document::AddImageShape(archive, *kept, "added");
    REQUIRE(other.has_value());

    const auto removed = Document::RemoveAnimation(archive, *fresh);
    INFO(Error(removed));
    REQUIRE(removed.has_value());
    CHECK(ListedNames(archive) == std::vector<std::vector<uint8_t>>{Bytes("intro"), Bytes("kept")});
    std::vector<std::string> expected{"afplist_Exml", Ifs::HashedName("intro"),
                                      "bsi/" + Ifs::HashedName("intro"), Ifs::HashedName("kept"),
                                      "bsi/" + Ifs::HashedName("kept")};
    std::vector<std::string> left = StoredNames(archive, "afp");
    std::ranges::sort(expected);
    std::ranges::sort(left);
    CHECK(left == expected);
    CHECK(StoredNames(archive, "geo") == std::vector<std::string>{Ifs::HashedName(
                                             "kept_shape" + std::to_string(other.value_or(0)))});
}

TEST_CASE("An animation another one imports, or one that is not listed, is not removed") {
    Ifs::Archive archive = Package();
    const std::string intro = "afp/" + HashPath("intro");
    const auto fresh = Document::AddAnimation(archive, "fresh", intro, 4);
    REQUIRE(fresh.has_value());
    if (!fresh) return;
    AfpAnimation::Animation importer = SamplePackage::SampleAnimation();
    importer.strings = {"", "fresh", "part"};
    importer.imports = {AfpAnimation::Import{
        .movie = 1, .assets = {AfpAnimation::ImportedAsset{.tag = 1, .name = 2}}}};
    const auto stored = AfpAnimation::WriteStored(importer);
    REQUIRE(stored.has_value());
    if (!stored) return;
    REQUIRE(Document::ReplaceEntry(archive, intro, stored->data).has_value());
    REQUIRE(Document::ReplaceEntry(archive, "afp/bsi/" + HashPath("intro"), stored->script)
                .has_value());

    const auto before = Ifs::Write(archive);
    REQUIRE(before.has_value());
    const auto imported = Document::RemoveAnimation(archive, *fresh);
    REQUIRE_FALSE(imported.has_value());
    CHECK(imported.error().find("intro") != std::string::npos);
    CHECK_FALSE(Document::RemoveAnimation(archive, "afp/" + HashPath("nothing")).has_value());
    CHECK_FALSE(Document::RemoveAnimation(archive, "tex/texturelist.xml").has_value());
    const auto after = Ifs::Write(archive);
    REQUIRE(after.has_value());
    CHECK(*after == *before);
    CHECK(Document::RemoveAnimation(archive, intro).has_value());
}
