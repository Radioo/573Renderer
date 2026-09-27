#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "sample_package.h"

#include "document/animation_strings.h"
#include "document/document.h"
#include "document/image_shape.h"
#include "document/span_transplant.h"
#include "formats/afp_animation.h"
#include "formats/ifs_archive.h"
#include "formats/ifs_names.h"

#include <cstddef>
#include <algorithm>
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
constexpr uint16_t kShape = 5;
constexpr uint16_t kMask = 6;
constexpr uint16_t kCard = 12;
constexpr uint16_t kGridShape = 20;
constexpr uint16_t kInnerGridShape = 21;
const std::vector<uint8_t> kShapeBytes{1, 2, 3, 4};

AfpAnimation::Tag Placed(uint16_t depth, uint16_t character) {
    AfpAnimation::Placement placement;
    placement.flags = kUseMatrix;
    placement.depth = depth;
    placement.character = character;
    return AfpAnimation::Tag{placement};
}

AfpAnimation::Sprite OneFrameSprite(uint16_t id, AfpAnimation::Tag placed) {
    AfpAnimation::Sprite sprite{.id = id, .container = {}};
    sprite.container.frames = {AfpAnimation::Frame{.first_tag = 0, .tag_count = 1}};
    sprite.container.tags = {std::move(placed)};
    return sprite;
}

void Grid(AfpAnimation::Tag& tag, uint16_t shape) {
    auto& placement = std::get<AfpAnimation::Placement>(tag.body);
    placement.extended_flags = 0;
    placement.grid_controller = AfpAnimation::GridController{.tag = shape, .first = 0, .second = 0};
}

AfpAnimation::Animation Intro(bool gridded) {
    AfpAnimation::Animation intro = SamplePackage::SampleAnimation();
    intro.flags = 0xC3;
    intro.strings = {"",          "intro", "aeplib", "__Packages.aeplib", "aep_mask_dummy",
                     "aeplibset", "wave"};
    intro.name = 1;
    intro.imports = {AfpAnimation::Import{
        .movie = 2, .assets = {AfpAnimation::ImportedAsset{.tag = kImported, .name = 3}}}};
    intro.import_initializers = AfpAnimation::ImportInitializers{
        .leading_word = 0,
        .entries = {AfpAnimation::ImportInitializer{.tag = kImported, .frame = 0}}};
    intro.root.labels.clear();
    AfpAnimation::Sprite card = OneFrameSprite(kCard, Placed(1, kShape));
    card.container.labels = {AfpAnimation::Label{.frame = 0, .name = 6}};
    AfpAnimation::Tag carded = Placed(5, kCard);
    if (gridded) {
        Grid(carded, kGridShape);
        Grid(card.container.tags[0], kInnerGridShape);
    }
    intro.root.tags = {AfpAnimation::Tag{OneFrameSprite(kLibrary, Placed(0, kImported))},
                       AfpAnimation::Tag{AfpAnimation::Shape{.unread_word = 0, .id = kShape}},
                       AfpAnimation::Tag{OneFrameSprite(kMask, Placed(1, kShape))},
                       AfpAnimation::Tag{card},
                       carded,
                       Placed(7, kLibrary)};
    if (gridded) {
        for (const uint16_t shape : {kGridShape, kInnerGridShape}) {
            intro.root.tags.insert(intro.root.tags.begin(), AfpAnimation::Tag{AfpAnimation::Shape{
                                                                .unread_word = 0, .id = shape}});
        }
    }
    const auto defined = static_cast<uint32_t>(intro.root.tags.size());
    intro.root.frames = {AfpAnimation::Frame{.first_tag = 0, .tag_count = defined},
                         AfpAnimation::Frame{.first_tag = defined, .tag_count = 0},
                         AfpAnimation::Frame{.first_tag = defined, .tag_count = 0}};
    intro.exports = {AfpAnimation::Export{.tag = kMask, .name = 4},
                     AfpAnimation::Export{.tag = kLibrary, .name = 5}};
    return intro;
}

std::optional<Document::File> Package(bool gridded = false) {
    const auto stored = AfpAnimation::WriteStored(Intro(gridded));
    REQUIRE(stored.has_value());
    Ifs::Archive archive = SamplePackage::SampleArchive();
    for (Ifs::Entry& directory : archive.entries) {
        if (directory.name != "afp") continue;
        directory.children = {
            SamplePackage::File("afplist_Exml", SamplePackage::ListWithGeo("intro", {kShape})),
            SamplePackage::File(Ifs::HashedName("intro"), stored->data),
            SamplePackage::Directory(
                "bsi", {SamplePackage::File(Ifs::HashedName("intro"), stored->script)})};
    }
    archive.entries.push_back(SamplePackage::Directory(
        "geo", {SamplePackage::File(Ifs::HashedName("intro_shape5"), kShapeBytes)}));
    const auto bytes = Ifs::Write(archive);
    REQUIRE(bytes.has_value());
    auto file = Document::File::Open(*bytes);
    REQUIRE(file.has_value());
    if (!file) return std::nullopt;
    return std::move(*file);
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

const AfpAnimation::Sprite* SpriteWith(const AfpAnimation::Animation& animation, uint16_t id) {
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* sprite = std::get_if<AfpAnimation::Sprite>(&tag.body);
        if (sprite != nullptr && sprite->id == id) return sprite;
    }
    return nullptr;
}

std::optional<uint16_t> PlacedAt(const AfpAnimation::Container& clip, uint16_t depth) {
    for (const AfpAnimation::Tag& tag : clip.tags) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement != nullptr && placement->depth == depth) return placement->character;
    }
    return std::nullopt;
}

}

TEST_CASE("A depth pasted into another animation brings its sprites and shapes along") {
    auto file = Package();
    if (!file) return;
    const std::string intro = "afp/" + HashPath("intro");
    const auto other = file->AddAnimation("other", *file, intro, 3);
    INFO(Error(other));
    REQUIRE(other.has_value());
    if (!other) return;
    const auto before = file->ReadAnimation(*other);
    REQUIRE(before.has_value());
    if (!before) return;
    const std::size_t defined_before = before->root.frames.at(0).tag_count;

    const auto copied = Document::CopySpanFrom(*file, intro, {}, 5, 0);
    INFO(Error(copied));
    REQUIRE(copied.has_value());
    if (!copied) return;
    const auto pasted = Document::PasteSpanInto(*file, *other, {}, *copied, 4, 0);
    INFO(Error(pasted));
    REQUIRE(pasted.has_value());

    const auto after = file->ReadAnimation(*other);
    REQUIRE(after.has_value());
    if (!after) return;
    CHECK(after->root.frames.at(0).tag_count == defined_before + 3);
    const std::optional<uint16_t> card = PlacedAt(after->root, 4);
    REQUIRE(card.has_value());
    if (!card) return;
    CHECK(SpriteWith(*before, *card) == nullptr);
    const AfpAnimation::Sprite* sprite = SpriteWith(*after, *card);
    REQUIRE(sprite != nullptr);
    if (sprite == nullptr) return;
    const std::optional<uint16_t> shape = PlacedAt(sprite->container, 1);
    REQUIRE(shape.has_value());
    if (!shape) return;
    CHECK(Document::NextCharacterId(*before) == *shape);
    CHECK(*shape != *card);
    REQUIRE(sprite->container.labels.size() == 1);
    CHECK(Document::StringText(*after, sprite->container.labels[0].name) == "wave");
    CHECK(file->ShapeFile(*other, *shape) == kShapeBytes);
    CHECK(file->ShapeFile(*other, kShape).has_value());
    CHECK(file->ShapeFile(intro, kShape) == kShapeBytes);
}

TEST_CASE("A grid controller pasted into another animation names a definition it brought along") {
    auto file = Package(true);
    if (!file) return;
    const std::string intro = "afp/" + HashPath("intro");
    const auto other = file->AddAnimation("other", *file, intro, 3);
    REQUIRE(other.has_value());
    if (!other) return;
    const auto before = file->ReadAnimation(*other);
    REQUIRE(before.has_value());
    if (!before) return;

    const auto copied = Document::CopySpanFrom(*file, intro, {}, 5, 0);
    REQUIRE(copied.has_value());
    if (!copied) return;
    const auto pasted = Document::PasteSpanInto(*file, *other, {}, *copied, 4, 0);
    INFO(Error(pasted));
    REQUIRE(pasted.has_value());

    const auto after = file->ReadAnimation(*other);
    REQUIRE(after.has_value());
    if (!after) return;
    const auto grid_at = [](const AfpAnimation::Container& clip,
                            uint16_t depth) -> std::optional<uint16_t> {
        for (const AfpAnimation::Tag& tag : clip.tags) {
            const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
            if (placement != nullptr && placement->depth == depth && placement->grid_controller)
                return placement->grid_controller->tag;
        }
        return std::nullopt;
    };
    const std::optional<uint16_t> grid = grid_at(after->root, 4);
    REQUIRE(grid.has_value());
    if (!grid) return;
    const auto defines_shape = [](const AfpAnimation::Animation& animation, uint16_t id) {
        return std::ranges::any_of(animation.root.tags, [id](const AfpAnimation::Tag& tag) {
            const auto* shape = std::get_if<AfpAnimation::Shape>(&tag.body);
            return shape != nullptr && shape->id == id;
        });
    };
    CHECK(defines_shape(*after, *grid));
    CHECK(*grid >= Document::NextCharacterId(*before).value_or(0));
    const std::optional<uint16_t> card = PlacedAt(after->root, 4);
    REQUIRE(card.has_value());
    if (!card) return;
    const AfpAnimation::Sprite* sprite = SpriteWith(*after, *card);
    REQUIRE(sprite != nullptr);
    if (sprite == nullptr) return;
    const std::optional<uint16_t> inner = grid_at(sprite->container, 1);
    REQUIRE(inner.has_value());
    if (!inner) return;
    CHECK(defines_shape(*after, *inner));
    CHECK(*inner != *grid);
    CHECK(*inner >= Document::NextCharacterId(*before).value_or(0));
}

TEST_CASE("A depth that needs an imported character is not pasted into another animation") {
    auto file = Package();
    if (!file) return;
    const std::string intro = "afp/" + HashPath("intro");
    const auto other = file->AddAnimation("other", *file, intro, 3);
    REQUIRE(other.has_value());
    if (!other) return;
    const auto copied = Document::CopySpanFrom(*file, intro, {}, 7, 0);
    REQUIRE(copied.has_value());
    if (!copied) return;
    const auto before = file->Encode();
    const auto pasted = Document::PasteSpanInto(*file, *other, {}, *copied, 4, 0);
    REQUIRE_FALSE(pasted.has_value());
    CHECK(pasted.error().find("import") != std::string::npos);
    CHECK(file->Encode() == before);
}

TEST_CASE("Pasting into the same animation keeps using its own characters") {
    auto file = Package();
    if (!file) return;
    const std::string intro = "afp/" + HashPath("intro");
    const auto copied = Document::CopySpanFrom(*file, intro, {}, 5, 0);
    REQUIRE(copied.has_value());
    if (!copied) return;
    const auto pasted = Document::PasteSpanInto(*file, intro, {}, *copied, 8, 0);
    INFO(Error(pasted));
    REQUIRE(pasted.has_value());
    const auto after = file->ReadAnimation(intro);
    REQUIRE(after.has_value());
    if (!after) return;
    CHECK(PlacedAt(after->root, 8) == kCard);
}
