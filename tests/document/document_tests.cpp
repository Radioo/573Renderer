#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sample_package.h"

#include "document/document.h"
#include "document/entry_edit.h"
#include "document/outline.h"
#include "formats/afp_animation.h"
#include "formats/ifs_archive.h"
#include "formats/ifs_names.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using SamplePackage::HashPath;
using SamplePackage::SampleArchive;

std::vector<uint8_t> SampleBytes() {
    const auto bytes = Ifs::Write(SampleArchive());
    const std::string error = bytes.has_value() ? std::string() : bytes.error();
    INFO(error);
    REQUIRE(bytes.has_value());
    return *bytes;
}

std::string AnimationPath() {
    return "afp/" + HashPath("intro");
}

}

TEST_CASE("An opened document shows the package") {
    auto file = Document::File::Open(SampleBytes());
    REQUIRE(file.has_value());
    CHECK(file->Problems().empty());
    CHECK(file->Nodes().size() == 4);
    const auto details = file->Describe(AnimationPath());
    REQUIRE(details.has_value());
    CHECK(details->name == "intro");
}

TEST_CASE("Opening bytes that are not an IFS is an error") {
    const std::vector<uint8_t> not_an_ifs(64, 0x5A);
    CHECK_FALSE(Document::File::Open(not_an_ifs).has_value());
}

TEST_CASE("A written animation comes back changed") {
    auto file = Document::File::Open(SampleBytes());
    REQUIRE(file.has_value());
    auto animation = file->ReadAnimation(AnimationPath());
    REQUIRE(animation.has_value());
    REQUIRE(animation->root.labels.size() == 1);

    animation->strings.emplace_back("second");
    animation->root.labels.push_back(AfpAnimation::Label{
        .frame = 1, .name = static_cast<uint32_t>(animation->strings.size() - 1)});
    REQUIRE(file->WriteAnimation(AnimationPath(), *animation).has_value());

    const auto details = file->Describe(AnimationPath());
    REQUIRE(details.has_value());
    REQUIRE(details->animation.has_value());
    const Document::AnimationDetails read =
        details->animation.value_or(Document::AnimationDetails{});
    REQUIRE(read.labels.size() == 2);
    CHECK(read.labels[1].name == "second");
    CHECK(read.labels[1].frame == 1);
}

TEST_CASE("An encoded document reopens with the change and nothing else touched") {
    auto file = Document::File::Open(SampleBytes());
    REQUIRE(file.has_value());
    auto animation = file->ReadAnimation(AnimationPath());
    REQUIRE(animation.has_value());
    animation->root.frames.push_back(AfpAnimation::Frame{});
    REQUIRE(file->WriteAnimation(AnimationPath(), *animation).has_value());

    const auto encoded = file->Encode();
    REQUIRE(encoded.has_value());
    auto reopened = Document::File::Open(*encoded);
    REQUIRE(reopened.has_value());

    const auto details = reopened->Describe(AnimationPath());
    REQUIRE(details.has_value());
    const Document::AnimationDetails read =
        details->animation.value_or(Document::AnimationDetails{});
    CHECK(read.frame_count == 4);

    const auto original = Ifs::Read(SampleBytes());
    const auto written = Ifs::Read(*encoded);
    REQUIRE(original.has_value());
    REQUIRE(written.has_value());
    REQUIRE(original->entries.size() == written->entries.size());
    CHECK(original->entries[0].bytes == written->entries[0].bytes);
    REQUIRE(original->entries[1].children.size() == written->entries[1].children.size());
    CHECK(original->entries[1].children[1].bytes == written->entries[1].children[1].bytes);
}

TEST_CASE("Writing an animation to a path that is not one is an error") {
    auto file = Document::File::Open(SampleBytes());
    REQUIRE(file.has_value());
    const auto animation = file->ReadAnimation(AnimationPath());
    REQUIRE(animation.has_value());
    CHECK_FALSE(file->WriteAnimation("magic", *animation).has_value());
    CHECK_FALSE(file->ReadAnimation("magic").has_value());
}

TEST_CASE("An entry is added under the hash of the name it was given") {
    auto file = Document::File::Open(SampleBytes());
    REQUIRE(file.has_value());
    const std::vector<uint8_t> bytes(32, 0x7F);
    REQUIRE(file->AddEntry("afp/bsi", "second", bytes).has_value());

    const auto details = file->Describe("afp/bsi/" + HashPath("second"));
    REQUIRE(details.has_value());
    CHECK(details->stored_name == Ifs::HashedName("second"));
    CHECK(details->stored_size == 32);
    CHECK(details->role == Document::Role::ByteOrderScript);
    CHECK_FALSE(file->AddEntry("afp/bsi", "second", bytes).has_value());
    CHECK_FALSE(file->AddEntry("nowhere", "second", bytes).has_value());
    CHECK_FALSE(file->AddEntry("afp/bsi", "", bytes).has_value());
}

TEST_CASE("An entry outside a hashed directory keeps its escaped name") {
    auto file = Document::File::Open(SampleBytes());
    REQUIRE(file.has_value());
    REQUIRE(file->AddEntry("", "notes.txt", std::vector<uint8_t>{1, 2, 3}).has_value());
    const auto details = file->Describe("notes.txt");
    REQUIRE(details.has_value());
    CHECK(details->stored_name == "notes_Etxt");
}

TEST_CASE("An entry is replaced and removed by its path") {
    auto file = Document::File::Open(SampleBytes());
    REQUIRE(file.has_value());
    REQUIRE(file->ReplaceEntry("magic", std::vector<uint8_t>{'N', 'G', 'N', 'N'}).has_value());
    const auto details = file->Describe("magic");
    REQUIRE(details.has_value());
    CHECK(details->stored_size == 4);

    REQUIRE(file->RemoveEntry("magic").has_value());
    CHECK_FALSE(file->Describe("magic").has_value());
    CHECK_FALSE(file->RemoveEntry("magic").has_value());
    CHECK_FALSE(file->ReplaceEntry("magic", std::vector<uint8_t>{1}).has_value());
}

TEST_CASE("An added entry survives an encode and a reopen") {
    auto file = Document::File::Open(SampleBytes());
    REQUIRE(file.has_value());
    REQUIRE(file->AddEntry("afp/bsi", "extra", std::vector<uint8_t>(8, 0x11)).has_value());
    const auto encoded = file->Encode();
    REQUIRE(encoded.has_value());
    const auto reopened = Document::File::Open(*encoded);
    REQUIRE(reopened.has_value());
    const auto details = reopened->Describe("afp/bsi/" + HashPath("extra"));
    REQUIRE(details.has_value());
    CHECK(details->stored_size == 8);
}

TEST_CASE("An image is added with its own texture and its pixels") {
    auto file = Document::File::Open(SampleBytes());
    REQUIRE(file.has_value());
    const std::vector<uint8_t> bgra(static_cast<std::size_t>(4) * 3 * 4, 0x40);
    const auto added = file->AddImage("added", 4, 3, bgra);
    const std::string error = added.has_value() ? std::string() : added.error();
    INFO(error);
    REQUIRE(added.has_value());

    CHECK(file->Problems().empty());
    const auto details = file->Describe("tex/" + HashPath("added"));
    REQUIRE(details.has_value());
    REQUIRE(details->texture.has_value());
    const Document::TextureDetails image = details->texture.value_or(Document::TextureDetails{});
    CHECK(image.width == 4);
    CHECK(image.height == 3);
    CHECK(image.format == "argb8888rev");
    CHECK(details->name == "added");
}

TEST_CASE("An image the package cannot take is refused") {
    auto file = Document::File::Open(SampleBytes());
    REQUIRE(file.has_value());
    CHECK_FALSE(file->AddImage("bg03", 4, 3, std::vector<uint8_t>(48, 0)).has_value());
    CHECK_FALSE(file->AddImage("tiny", 1, 1, std::vector<uint8_t>(4, 0)).has_value());
    CHECK_FALSE(file->AddImage("short", 4, 3, std::vector<uint8_t>(8, 0)).has_value());
}

TEST_CASE("A removed image leaves the list and the entry") {
    auto file = Document::File::Open(SampleBytes());
    REQUIRE(file.has_value());
    REQUIRE(file->RemoveImage("bg03").has_value());
    CHECK(file->Problems().empty());
    CHECK_FALSE(file->Describe("tex/" + HashPath("bg03")).has_value());
    CHECK_FALSE(file->RemoveImage("bg03").has_value());
}

TEST_CASE("An added image survives an encode and a reopen") {
    auto file = Document::File::Open(SampleBytes());
    REQUIRE(file.has_value());
    REQUIRE(file->AddImage("added", 4, 3, std::vector<uint8_t>(48, 0x40)).has_value());
    const auto encoded = file->Encode();
    REQUIRE(encoded.has_value());
    const auto reopened = Document::File::Open(*encoded);
    REQUIRE(reopened.has_value());
    CHECK(reopened->Problems().empty());
    const auto details = reopened->Describe("tex/" + HashPath("added"));
    REQUIRE(details.has_value());
    CHECK(details->name == "added");
}

TEST_CASE("Package list files keep readable names while their siblings are hashed") {
    CHECK(Document::StoredName("afp", "afplist.xml") == std::string("afplist_Exml"));
    CHECK(Document::StoredName("tex", "texturelist.xml") == std::string("texturelist_Exml"));
    CHECK(Document::StoredName("afp", "intro") == Ifs::HashedName("intro"));
    CHECK(Document::StoredName("afp/bsi", "afplist.xml") == Ifs::HashedName("afplist.xml"));
}
