#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sample_package.h"

#include "document/animation_strings.h"
#include "document/document.h"
#include "document/image_shape.h"
#include "document/place_image.h"
#include "document/placement_edit.h"
#include "document/stage_bounds.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "formats/big_endian.h"
#include "formats/binary_xml.h"
#include "formats/ge2d_shape.h"
#include "formats/ifs_archive.h"
#include "formats/ifs_names.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

using SamplePackage::HashPath;

constexpr uint32_t kMeshFlags = 0x20;
constexpr uint16_t kTexturedShapeWord = 2;

std::string AnimationPath() {
    return "afp/" + HashPath("intro");
}

uint32_t Bits(float value) {
    return std::bit_cast<uint32_t>(value);
}

std::optional<Document::File> NamedPackage() {
    const auto bytes = Ifs::Write(SamplePackage::SampleArchive());
    REQUIRE(bytes.has_value());
    auto file = Document::File::Open(*bytes);
    REQUIRE(file.has_value());
    auto animation = file->ReadAnimation(AnimationPath());
    REQUIRE(animation.has_value());
    animation->name = Document::InternString(*animation, "intro");
    animation->root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Shape{.unread_word = kTexturedShapeWord, .id = 4}});
    animation->root.frames[0].tag_count = 1;
    for (std::size_t i = 1; i < animation->root.frames.size(); i++)
        animation->root.frames[i].first_tag = 1;
    REQUIRE(file->WriteAnimation(AnimationPath(), *animation).has_value());
    REQUIRE(file->AddImage("added", 4, 3, std::vector<uint8_t>(48, 0x40)).has_value());
    return std::move(*file);
}

std::vector<uint16_t> ListedShapes(const Document::File& file) {
    const auto encoded = file.Encode();
    REQUIRE(encoded.has_value());
    const auto archive = Ifs::Read(*encoded);
    REQUIRE(archive.has_value());
    std::vector<uint16_t> ids;
    for (const Ifs::Entry& directory : archive->entries) {
        if (directory.name != "afp") continue;
        for (const Ifs::Entry& entry : directory.children) {
            if (entry.name != "afplist_Exml") continue;
            const auto list = BinaryXml::Read(entry.bytes);
            REQUIRE(list.has_value());
            for (const BinaryXml::Node& listed : list->root.children) {
                for (const BinaryXml::Node& geo : listed.children) {
                    for (std::size_t at = 0; at + 1 < geo.value.size(); at += 2)
                        ids.push_back(BigEndian::ReadU16(geo.value, at));
                }
            }
        }
    }
    return ids;
}

std::optional<Ge2dShape::Shape> WrittenShape(const Document::File& file, uint16_t id) {
    const auto encoded = file.Encode();
    REQUIRE(encoded.has_value());
    const auto archive = Ifs::Read(*encoded);
    REQUIRE(archive.has_value());
    const std::string stored = Ifs::HashedName("intro_shape" + std::to_string(id));
    for (const Ifs::Entry& directory : archive->entries) {
        if (directory.name != "geo") continue;
        const auto found = std::ranges::find(directory.children, stored, &Ifs::Entry::name);
        if (found == directory.children.end()) return std::nullopt;
        auto shape = Ge2dShape::Read(found->bytes, Ge2dShape::ByteOrder::Big);
        if (shape) return *shape;
    }
    return std::nullopt;
}

}

TEST_CASE("An image quad spans the image from the origin and samples its atlas area") {
    const Document::ImageArea area{.atlas_width = 8, .atlas_height = 4, .uvrect = {2, 10, 2, 6}};
    const Ge2dShape::Shape shape = Document::ImageQuad("star", area, false);

    CHECK(shape.flags == 0);
    CHECK_FALSE(shape.rect.has_value());
    CHECK(shape.vertices ==
          std::vector<std::array<uint32_t, 2>>{
              {Bits(0), Bits(0)}, {Bits(4), Bits(0)}, {Bits(0), Bits(2)}, {Bits(4), Bits(2)}});
    CHECK(shape.uvs == std::vector<std::array<uint32_t, 2>>{{Bits(0.125F), Bits(0.25F)},
                                                            {Bits(0.625F), Bits(0.25F)},
                                                            {Bits(0.125F), Bits(0.75F)},
                                                            {Bits(0.625F), Bits(0.75F)}});
    CHECK(shape.texture_names == std::vector<std::string>{"star"});
    REQUIRE(shape.primitives.size() == 1);
    CHECK(shape.primitives[0].kind == 4);
    CHECK(shape.primitives[0].draw_flags == 0x3);
    CHECK(shape.primitives[0].texture == 0);
    CHECK(shape.primitives[0].second_texture == 0xFF);
    CHECK(shape.primitives[0].indices == std::vector<uint16_t>{0, 1, 2, 2, 1, 3});
    CHECK(Ge2dShape::Write(shape, Ge2dShape::ByteOrder::Big).has_value());
}

TEST_CASE("An image quad in a mesh package carries its bounds") {
    const Document::ImageArea area{.atlas_width = 8, .atlas_height = 4, .uvrect = {2, 10, 2, 6}};
    const Ge2dShape::Shape shape = Document::ImageQuad("star", area, true);
    CHECK(shape.flags == kMeshFlags);
    CHECK(shape.rect == std::array<uint32_t, 4>{Bits(0), Bits(4), Bits(0), Bits(2)});
}

TEST_CASE("A new character takes the id above every character the animation knows") {
    AfpAnimation::Animation animation;
    CHECK(Document::NextCharacterId(animation) == uint16_t{0});

    animation.root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Sprite{.id = 5, .container = {}}});
    animation.root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Shape{.unread_word = 0, .id = 3}});
    animation.imports.push_back(AfpAnimation::Import{
        .movie = 0, .assets = {AfpAnimation::ImportedAsset{.tag = 9, .name = 0}}});
    CHECK(Document::NextCharacterId(animation) == uint16_t{10});

    animation.root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Shape{.unread_word = 0, .id = 0xFFFE}});
    CHECK_FALSE(Document::NextCharacterId(animation).has_value());
}

TEST_CASE("Adding an image shape defines it, writes its quad and lists it") {
    auto file = NamedPackage();
    if (!file) return;
    const auto added = file->AddImageShape(AnimationPath(), "added");
    const std::string error = added.has_value() ? std::string() : added.error();
    INFO(error);
    REQUIRE(added.has_value());
    if (!added) return;
    CHECK(*added == 5);
    CHECK(file->Problems().empty());

    const auto animation = file->ReadAnimation(AnimationPath());
    REQUIRE(animation.has_value());
    if (!animation) return;
    const AfpAnimation::Frame& first = animation->root.frames[0];
    REQUIRE(first.tag_count == 2);
    const auto* shape =
        std::get_if<AfpAnimation::Shape>(&animation->root.tags[first.first_tag + 1].body);
    REQUIRE(shape != nullptr);
    CHECK(shape->id == 5);
    CHECK(shape->unread_word == kTexturedShapeWord);

    CHECK(ListedShapes(*file) == std::vector<uint16_t>{5});
    CHECK(file->ShapeImages(AnimationPath()) ==
          std::map<uint16_t, std::string>{{uint16_t{5}, "added"}});
    CHECK(file->ShapeBounds(AnimationPath()) ==
          std::map<uint16_t, Document::Box>{
              {uint16_t{5}, Document::Box{.left = 0, .right = 2, .top = 0, .bottom = 1}}});
    const auto written = WrittenShape(*file, 5);
    REQUIRE(written.has_value());
    const Document::ImageArea area{.atlas_width = 4, .atlas_height = 3, .uvrect = {2, 6, 2, 4}};
    CHECK(written == Document::ImageQuad("added", area, false));
}

TEST_CASE("An image shape for an image the package lacks changes nothing") {
    auto file = NamedPackage();
    if (!file) return;
    const auto before = file->Encode();
    REQUIRE(before.has_value());
    CHECK_FALSE(file->AddImageShape(AnimationPath(), "missing").has_value());
    CHECK(file->Encode() == before);
}

TEST_CASE("An image shape needs a named, listed animation") {
    const auto bytes = Ifs::Write(SamplePackage::SampleArchive());
    REQUIRE(bytes.has_value());
    auto file = Document::File::Open(*bytes);
    REQUIRE(file.has_value());
    REQUIRE(file->AddImage("added", 4, 3, std::vector<uint8_t>(48, 0x40)).has_value());
    CHECK_FALSE(file->AddImageShape(AnimationPath(), "added").has_value());
    CHECK_FALSE(file->AddImageShape("afp/" + HashPath("other"), "added").has_value());
}

TEST_CASE("Placing an image defines its shape and places it on a new depth in one step") {
    auto file = NamedPackage();
    if (!file) return;
    const auto placed = Document::PlaceImage(
        *file, AnimationPath(), "added",
        Document::DepthSpan{.clip = {}, .depth = 7, .first_frame = 0, .last_frame = 1});
    const std::string error = placed.has_value() ? std::string() : placed.error();
    INFO(error);
    REQUIRE(placed.has_value());
    if (!placed) return;

    const auto animation = file->ReadAnimation(AnimationPath());
    REQUIRE(animation.has_value());
    if (!animation) return;
    const auto rows = Document::DepthRows(animation->root);
    const auto row = std::ranges::find(rows, uint16_t{7}, &Document::DepthRow::depth);
    REQUIRE(row != rows.end());
    REQUIRE(row->spans.size() == 1);
    CHECK(row->spans[0].last_frame == 1);
    const auto tag = Document::LivePlacementTag(animation->root, 7, 0);
    REQUIRE(tag.has_value());
    if (!tag) return;
    const auto* placement = std::get_if<AfpAnimation::Placement>(&animation->root.tags[*tag].body);
    REQUIRE(placement != nullptr);
    CHECK(placement->character == *placed);
    CHECK(ListedShapes(*file) == std::vector<uint16_t>{*placed});
}

TEST_CASE("A placement that cannot be made leaves the package as it was") {
    auto file = NamedPackage();
    if (!file) return;
    const auto before = file->Encode();
    REQUIRE(before.has_value());
    const auto placed = Document::PlaceImage(
        *file, AnimationPath(), "added",
        Document::DepthSpan{.clip = {}, .depth = 7, .first_frame = 2, .last_frame = 9});
    CHECK_FALSE(placed.has_value());
    CHECK(file->Encode() == before);
}
