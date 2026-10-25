#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/animation_strings.h"
#include "document/document.h"
#include "document/sprite_exports.h"
#include "document/tags.h"
#include "formats/afp_animation.h"
#include "formats/ifs_archive.h"
#include "sample_package.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr uint16_t kNamed = 10;
constexpr uint16_t kUnnamed = 11;
constexpr uint16_t kHelper = 12;
constexpr uint16_t kSelf = 13;

std::string IntroPath() {
    return "afp/" + SamplePackage::HashPath("intro");
}

std::optional<Document::File> Package() {
    const auto bytes = Ifs::Write(SamplePackage::SampleArchive());
    REQUIRE(bytes.has_value());
    auto file = Document::File::Open(*bytes);
    REQUIRE(file.has_value());
    if (!file) return std::nullopt;
    auto animation = file->ReadAnimation(IntroPath());
    REQUIRE(animation.has_value());
    if (!animation) return std::nullopt;
    animation->name = Document::InternString(*animation, "intro");
    for (const uint16_t id : {kNamed, kUnnamed, kHelper, kSelf}) {
        AfpAnimation::Sprite sprite{.id = id, .container = {}};
        sprite.container.frames.assign(1, AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
        Document::InsertTag(animation->root, 0, AfpAnimation::Tag{sprite});
    }
    Document::InsertExport(*animation, kNamed, "Thing");
    Document::InsertExport(*animation, kHelper, "aeplibset");
    Document::InsertExport(*animation, kSelf, "intro");
    REQUIRE(file->WriteAnimation(IntroPath(), *animation).has_value());
    return std::move(*file);
}

std::vector<std::string> Names(const Document::File& file) {
    const auto animation = file.ReadAnimation(IntroPath());
    REQUIRE(animation.has_value());
    std::vector<std::string> names;
    if (!animation) return names;
    for (const AfpAnimation::Export& exported : animation->exports)
        names.push_back(Document::StringText(*animation, exported.name));
    return names;
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

}

TEST_CASE("A sprite's export can be added, renamed and removed, and stays in lookup order") {
    auto file = Package();
    if (!file) return;
    const auto added = Document::NameSpriteExport(*file, IntroPath(), kUnnamed, "banner");
    INFO(Error(added));
    REQUIRE(added.has_value());
    CHECK(Names(*file) == std::vector<std::string>{"aeplibset", "banner", "intro", "Thing"});
    CHECK(Document::SpriteExportName(*file, IntroPath(), kUnnamed) == "banner");

    REQUIRE(Document::NameSpriteExport(*file, IntroPath(), kNamed, "zone").has_value());
    CHECK(Names(*file) == std::vector<std::string>{"aeplibset", "banner", "intro", "zone"});

    REQUIRE(Document::NameSpriteExport(*file, IntroPath(), kNamed, "").has_value());
    CHECK(Names(*file) == std::vector<std::string>{"aeplibset", "banner", "intro"});
    CHECK(Document::SpriteExportName(*file, IntroPath(), kNamed).empty());
    const auto animation = file->ReadAnimation(IntroPath());
    REQUIRE(animation.has_value());
    if (!animation) return;
    CHECK(std::ranges::find(animation->strings, "Thing") == animation->strings.end());
    CHECK(std::ranges::find(animation->strings, "zone") == animation->strings.end());
}

TEST_CASE("Export names that would clash or break a lookup are refused") {
    auto file = Package();
    if (!file) return;
    const auto before = file->Encode();
    CHECK_FALSE(Document::NameSpriteExport(*file, IntroPath(), kUnnamed, "thing").has_value());
    CHECK_FALSE(Document::NameSpriteExport(*file, IntroPath(), kUnnamed, "AEPLIBSET").has_value());
    CHECK_FALSE(Document::NameSpriteExport(*file, IntroPath(), kUnnamed, "Intro").has_value());
    CHECK_FALSE(Document::NameSpriteExport(*file, IntroPath(), kUnnamed, "two words").has_value());
    CHECK_FALSE(Document::NameSpriteExport(*file, IntroPath(), kHelper, "renamed").has_value());
    CHECK_FALSE(Document::NameSpriteExport(*file, IntroPath(), kSelf, "").has_value());
    CHECK_FALSE(Document::NameSpriteExport(*file, IntroPath(), 99, "missing").has_value());
    CHECK(file->Encode() == before);
}

TEST_CASE("An export another animation imports keeps its name") {
    auto file = Package();
    if (!file) return;
    const auto other = file->AddAnimation("other", *file, IntroPath(), 3);
    INFO(Error(other));
    REQUIRE(other.has_value());
    if (!other) return;
    auto importing = file->ReadAnimation(*other);
    REQUIRE(importing.has_value());
    if (!importing) return;
    importing->imports.push_back(
        AfpAnimation::Import{.movie = Document::InternString(*importing, "INTRO"),
                             .assets = {AfpAnimation::ImportedAsset{
                                 .tag = 40, .name = Document::InternString(*importing, "thing")}}});
    REQUIRE(file->WriteAnimation(*other, *importing).has_value());

    const auto renamed = Document::NameSpriteExport(*file, IntroPath(), kNamed, "zone");
    REQUIRE_FALSE(renamed.has_value());
    CHECK(renamed.error().find("other") != std::string::npos);
    CHECK_FALSE(Document::NameSpriteExport(*file, IntroPath(), kNamed, "").has_value());
    CHECK(Document::NameSpriteExport(*file, IntroPath(), kUnnamed, "banner").has_value());
}
