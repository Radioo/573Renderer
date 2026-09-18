#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sample_package.h"

#include "document/document.h"
#include "document/entries.h"
#include "document/unused_definitions.h"
#include "formats/afp_animation.h"
#include "formats/big_endian.h"
#include "formats/binary_xml.h"
#include "formats/ifs_archive.h"
#include "formats/ifs_names.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

using SamplePackage::HashPath;

constexpr uint32_t kUseMatrix = 0x4;
constexpr uint16_t kImported = 2;
constexpr uint16_t kLibrary = 3;
constexpr uint16_t kMaskShape = 5;
constexpr uint16_t kMask = 6;
constexpr uint16_t kCard = 9;
constexpr uint16_t kCardShape = 10;
constexpr uint16_t kLoose = 11;
constexpr uint16_t kLooseShape = 12;
constexpr uint16_t kLooseHolder = 13;
constexpr uint16_t kBareShape = 14;
constexpr uint16_t kGridShape = 15;
const std::vector<uint8_t> kShapeBytes{1, 2, 3, 4};

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

AfpAnimation::Tag Placed(uint16_t depth, uint16_t character) {
    AfpAnimation::Placement placement;
    placement.flags = kUseMatrix;
    placement.depth = depth;
    placement.character = character;
    return AfpAnimation::Tag{placement};
}

AfpAnimation::Tag Holding(uint16_t id, uint16_t character) {
    AfpAnimation::Sprite sprite{.id = id, .container = {}};
    sprite.container.frames = {AfpAnimation::Frame{.first_tag = 0, .tag_count = 1}};
    sprite.container.tags = {Placed(1, character)};
    return AfpAnimation::Tag{std::move(sprite)};
}

AfpAnimation::Tag ShapeTag(uint16_t id) {
    return AfpAnimation::Tag{AfpAnimation::Shape{.unread_word = 0, .id = id}};
}

AfpAnimation::Animation Intro() {
    AfpAnimation::Animation intro = SamplePackage::SampleAnimation();
    intro.flags = 0xC3;
    intro.strings = {"", "intro", "aeplib", "__Packages.aeplib", "aep_mask_dummy", "aeplibset"};
    intro.name = 1;
    intro.imports = {AfpAnimation::Import{
        .movie = 2, .assets = {AfpAnimation::ImportedAsset{.tag = kImported, .name = 3}}}};
    intro.import_initializers = AfpAnimation::ImportInitializers{
        .leading_word = 0,
        .entries = {AfpAnimation::ImportInitializer{.tag = kImported, .frame = 0}}};
    intro.root.labels.clear();
    intro.root.tags = {Holding(kLibrary, kImported), ShapeTag(kMaskShape),
                       Holding(kMask, kMaskShape),   ShapeTag(kCardShape),
                       Holding(kCard, kCardShape),   ShapeTag(kLooseShape),
                       Holding(kLoose, kLooseShape), Holding(kLooseHolder, kLoose),
                       ShapeTag(kBareShape),         Placed(5, kCard),
                       ShapeTag(kGridShape)};
    auto& gridded = std::get<AfpAnimation::Placement>(intro.root.tags[9].body);
    gridded.extended_flags = 0;
    gridded.grid_controller =
        AfpAnimation::GridController{.tag = kGridShape, .first = 0, .second = 0};
    const auto defined = static_cast<uint32_t>(intro.root.tags.size());
    intro.root.frames = {AfpAnimation::Frame{.first_tag = 0, .tag_count = defined},
                         AfpAnimation::Frame{.first_tag = defined, .tag_count = 0},
                         AfpAnimation::Frame{.first_tag = defined, .tag_count = 0}};
    intro.exports = {AfpAnimation::Export{.tag = kMask, .name = 4},
                     AfpAnimation::Export{.tag = kLibrary, .name = 5}};
    return intro;
}

std::optional<Document::File> Package(const AfpAnimation::Animation& intro) {
    const auto stored = AfpAnimation::WriteStored(intro);
    INFO(Error(stored));
    REQUIRE(stored.has_value());
    Ifs::Archive archive = SamplePackage::SampleArchive();
    for (Ifs::Entry& directory : archive.entries) {
        if (directory.name != "afp") continue;
        directory.children = {
            SamplePackage::File("afplist_Exml",
                                SamplePackage::ListWithGeo(
                                    "intro", {kMaskShape, kCardShape, kLooseShape, kBareShape})),
            SamplePackage::File(Ifs::HashedName("intro"), stored->data),
            SamplePackage::Directory(
                "bsi", {SamplePackage::File(Ifs::HashedName("intro"), stored->script)})};
    }
    std::vector<Ifs::Entry> shapes;
    for (const uint16_t id : {kMaskShape, kCardShape, kLooseShape}) {
        shapes.push_back(
            SamplePackage::File(Ifs::HashedName("intro_shape" + std::to_string(id)), kShapeBytes));
    }
    archive.entries.push_back(SamplePackage::Directory("geo", std::move(shapes)));
    const auto bytes = Ifs::Write(archive);
    REQUIRE(bytes.has_value());
    auto file = Document::File::Open(*bytes);
    REQUIRE(file.has_value());
    if (!file) return std::nullopt;
    return std::move(*file);
}

std::vector<uint16_t> Defined(const AfpAnimation::Animation& animation) {
    std::vector<uint16_t> ids;
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        if (const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body))
            ids.push_back(sprite->id);
        if (const auto* shape = std::get_if<AfpAnimation::Shape>(&tag.body))
            ids.push_back(shape->id);
    }
    std::ranges::sort(ids);
    return ids;
}

std::vector<uint16_t> ListedShapes(const Document::File& file) {
    const auto encoded = file.Encode();
    REQUIRE(encoded.has_value());
    const auto archive = Ifs::Read(*encoded);
    REQUIRE(archive.has_value());
    const Ifs::Entry* list = Document::FindEntry(*archive, "afp/afplist.xml");
    REQUIRE(list != nullptr);
    const auto document = BinaryXml::Read(list->bytes);
    REQUIRE(document.has_value());
    std::vector<uint16_t> shapes;
    for (const BinaryXml::Node& listed : document->root.children) {
        for (const BinaryXml::Node& geo : listed.children) {
            if (geo.name != "geo") continue;
            for (std::size_t at = 0; at + 2 <= geo.value.size(); at += 2)
                shapes.push_back(BigEndian::ReadU16(geo.value, at));
        }
    }
    return shapes;
}

}

TEST_CASE("Definitions nothing places, exports or imports are removed with their shape files") {
    auto file = Package(Intro());
    if (!file) return;
    const std::string intro = "afp/" + HashPath("intro");
    auto removed = Document::RemoveUnusedDefinitions(*file, intro);
    INFO(Error(removed));
    REQUIRE(removed.has_value());
    std::ranges::sort(*removed);
    CHECK(*removed == std::vector<uint16_t>{kLoose, kLooseShape, kLooseHolder, kBareShape});

    const auto after = file->ReadAnimation(intro);
    REQUIRE(after.has_value());
    CHECK(Defined(*after) ==
          std::vector<uint16_t>{kLibrary, kMaskShape, kMask, kCard, kCardShape, kGridShape});
    CHECK(after->root.frames.at(0).tag_count == 7);
    CHECK(after->root.frames.at(1).first_tag == 7);
    CHECK_FALSE(file->ShapeFile(intro, kLooseShape).has_value());
    CHECK(file->ShapeFile(intro, kCardShape) == kShapeBytes);
    CHECK(file->ShapeFile(intro, kMaskShape) == kShapeBytes);
    CHECK(ListedShapes(*file) == std::vector<uint16_t>{kMaskShape, kCardShape});
    CHECK(file->Problems().empty());

    const auto again = Document::RemoveUnusedDefinitions(*file, intro);
    REQUIRE(again.has_value());
    CHECK(again->empty());
}

TEST_CASE("An animation with a sprite defined inside a sprite is left alone") {
    AfpAnimation::Animation intro = Intro();
    auto& holder = std::get<AfpAnimation::Sprite>(intro.root.tags[7].body);
    holder.container.tags.insert(holder.container.tags.begin(),
                                 Holding(kGridShape + 1, kBareShape));
    holder.container.frames[0].tag_count = 2;
    auto file = Package(intro);
    if (!file) return;
    const std::string path = "afp/" + HashPath("intro");
    const auto before = file->Encode();
    const auto removed = Document::RemoveUnusedDefinitions(*file, path);
    REQUIRE_FALSE(removed.has_value());
    CHECK(removed.error().find("inside itself") != std::string::npos);
    CHECK(file->Encode() == before);
}
