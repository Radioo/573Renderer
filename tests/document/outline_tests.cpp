#include <catch2/catch_test_macros.hpp>

#include "sample_package.h"

#include "document/outline.h"
#include "formats/ifs_archive.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace {

using SamplePackage::HashPath;
using SamplePackage::SampleArchive;

const Document::Node* Find(const std::vector<Document::Node>& nodes, const std::string& path) {
    for (const Document::Node& node : nodes) {
        if (node.path == path) return &node;
        if (const Document::Node* found = Find(node.children, path); found != nullptr) return found;
    }
    return nullptr;
}

}

TEST_CASE("The outline names entries by where they sit in the package") {
    const Ifs::Archive archive = SampleArchive();
    const Document::Outline outline = Document::Outline::Build(archive);
    CHECK(outline.Problems().empty());

    const Document::Node* magic = Find(outline.Nodes(), "magic");
    REQUIRE(magic != nullptr);
    CHECK(magic->role == Document::Role::PackageMagic);

    const Document::Node* textures = Find(outline.Nodes(), "tex");
    REQUIRE(textures != nullptr);
    CHECK(textures->kind == Ifs::EntryKind::Directory);
    CHECK(textures->role == Document::Role::Unknown);

    const Document::Node* list = Find(outline.Nodes(), "tex/texturelist.xml");
    REQUIRE(list != nullptr);
    CHECK(list->role == Document::Role::TextureList);
    CHECK(list->stored_name == "texturelist_Exml");

    const Document::Node* image = Find(outline.Nodes(), "tex/" + HashPath("bg03"));
    REQUIRE(image != nullptr);
    CHECK(image->role == Document::Role::Texture);
    CHECK(image->name == "bg03");

    const Document::Node* animation = Find(outline.Nodes(), "afp/" + HashPath("intro"));
    REQUIRE(animation != nullptr);
    CHECK(animation->role == Document::Role::Animation);
    CHECK(animation->name == "intro");

    const Document::Node* script = Find(outline.Nodes(), "afp/bsi/" + HashPath("intro"));
    REQUIRE(script != nullptr);
    CHECK(script->role == Document::Role::ByteOrderScript);
    CHECK(script->name == "intro");

    const Document::Node* info = Find(outline.Nodes(), "_info_");
    REQUIRE(info != nullptr);
    CHECK(info->kind == Ifs::EntryKind::Special);
    CHECK(info->name == "_info_");
}

TEST_CASE("The outline marks a file that lives in a super image") {
    Ifs::Archive archive;
    Ifs::Entry entry = SamplePackage::File("held", {});
    entry.super_index = 2;
    entry.stored_size = 64;
    archive.entries.push_back(std::move(entry));
    const Document::Outline outline = Document::Outline::Build(archive);
    const Document::Node* node = Find(outline.Nodes(), "held");
    REQUIRE(node != nullptr);
    REQUIRE(node->super_index.has_value());
    CHECK(*node->super_index == 2);
}

TEST_CASE("Describing a texture gives its format and pixel size") {
    const Ifs::Archive archive = SampleArchive();
    const Document::Outline outline = Document::Outline::Build(archive);
    const auto details = outline.Describe(archive, "tex/" + HashPath("bg03"));
    REQUIRE(details.has_value());
    CHECK(details->role == Document::Role::Texture);
    CHECK(details->name == "bg03");
    CHECK(details->stored_size == 16);
    REQUIRE(details->texture.has_value());
    const Document::TextureDetails texture = details->texture.value_or(Document::TextureDetails{});
    CHECK(texture.format == "argb8888rev");
    CHECK(texture.width == 64);
    CHECK(texture.height == 32);
    CHECK_FALSE(details->animation.has_value());
}

TEST_CASE("Describing an animation gives its frame count and labels") {
    const Ifs::Archive archive = SampleArchive();
    const Document::Outline outline = Document::Outline::Build(archive);
    const auto details = outline.Describe(archive, "afp/" + HashPath("intro"));
    REQUIRE(details.has_value());
    REQUIRE(details->animation.has_value());
    const Document::AnimationDetails animation =
        details->animation.value_or(Document::AnimationDetails{});
    CHECK(animation.frame_count == 3);
    REQUIRE(animation.labels.size() == 1);
    CHECK(animation.labels[0].name == "loop");
    CHECK(animation.labels[0].frame == 2);
}

TEST_CASE("Describing an animation with no byte order script is an error") {
    Ifs::Archive archive = SampleArchive();
    Ifs::Entry& afp = archive.entries[2];
    afp.children.erase(afp.children.begin() + 2);
    const Document::Outline outline = Document::Outline::Build(archive);
    const auto details = outline.Describe(archive, "afp/" + HashPath("intro"));
    REQUIRE_FALSE(details.has_value());
    CHECK(details.error().find("byte order script") != std::string::npos);
}

TEST_CASE("Describing a path the package does not have is an error") {
    const Ifs::Archive archive = SampleArchive();
    const Document::Outline outline = Document::Outline::Build(archive);
    CHECK_FALSE(outline.Describe(archive, "tex/missing").has_value());
}

TEST_CASE("The outline reports an image the package never stored") {
    Ifs::Archive archive = SampleArchive();
    Ifs::Entry& tex = archive.entries[1];
    tex.children.erase(tex.children.begin() + 1);
    const Document::Outline outline = Document::Outline::Build(archive);
    REQUIRE(outline.Problems().size() == 1);
    CHECK(outline.Problems()[0].find("bg03") != std::string::npos);
}

TEST_CASE("Inspector fields name what the entry is") {
    const Ifs::Archive archive = SampleArchive();
    const Document::Outline outline = Document::Outline::Build(archive);
    const auto texture = outline.Describe(archive, "tex/" + HashPath("bg03"));
    REQUIRE(texture.has_value());
    const std::vector<Document::Field> fields = Document::Fields(*texture);
    CHECK(fields[0].name == "Name");
    CHECK(fields[0].value == "bg03");
    const auto format = std::ranges::find(fields, "Format", &Document::Field::name);
    REQUIRE(format != fields.end());
    CHECK(format->value == "argb8888rev");
    const auto pixels = std::ranges::find(fields, "Pixels", &Document::Field::name);
    REQUIRE(pixels != fields.end());
    CHECK(pixels->value == "64 x 32");
    CHECK(std::ranges::find(fields, "Frames", &Document::Field::name) == fields.end());

    const auto animation = outline.Describe(archive, "afp/" + HashPath("intro"));
    REQUIRE(animation.has_value());
    const std::vector<Document::Field> animation_fields = Document::Fields(*animation);
    const auto labels = std::ranges::find(animation_fields, "Labels", &Document::Field::name);
    REQUIRE(labels != animation_fields.end());
    CHECK(labels->value == "loop at 2");
}
