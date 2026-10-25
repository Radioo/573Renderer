#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/animation_strings.h"
#include "document/clip.h"
#include "document/document.h"
#include "document/sprite_preview.h"
#include "formats/afp_animation.h"
#include "formats/ifs_archive.h"

#include "sample_package.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr uint16_t kNamed = 5;
constexpr uint16_t kUnnamed = 9;

std::string Path() {
    return "afp/" + SamplePackage::HashPath("intro");
}

AfpAnimation::Animation Scene() {
    AfpAnimation::Animation animation = SamplePackage::SampleAnimation();
    AfpAnimation::Container frames;
    frames.frames.resize(3);
    animation.root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Sprite{.id = kNamed, .container = frames}});
    animation.root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Sprite{.id = kUnnamed, .container = frames}});
    animation.exports.clear();
    animation.exports.push_back(
        AfpAnimation::Export{.tag = kNamed, .name = Document::InternString(animation, "Spinner")});
    animation.exports.push_back(
        AfpAnimation::Export{.tag = 40, .name = Document::InternString(animation, "zoom")});
    return animation;
}

Document::File Package(const AfpAnimation::Animation& animation) {
    const auto bytes = Ifs::Write(SamplePackage::SampleArchive());
    REQUIRE(bytes.has_value());
    auto file = Document::File::Open(*bytes);
    REQUIRE(file.has_value());
    REQUIRE(file->WriteAnimation(Path(), animation).has_value());
    return std::move(*file);
}

std::string Folded(std::string text) {
    std::ranges::transform(text, text.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::vector<std::string> ExportNames(const AfpAnimation::Animation& animation) {
    std::vector<std::string> names;
    names.reserve(animation.exports.size());
    for (const AfpAnimation::Export& exported : animation.exports)
        names.push_back(Document::StringText(animation, exported.name));
    return names;
}

AfpAnimation::Animation Previewed(const Document::PreviewSymbol& preview) {
    auto opened = Document::File::Open(preview.ifs);
    REQUIRE(opened.has_value());
    auto animation = opened->ReadAnimation(Path());
    REQUIRE(animation.has_value());
    return std::move(*animation);
}

}

TEST_CASE("An exported sprite previews under its own name with the package unchanged") {
    const Document::File file = Package(Scene());
    const auto preview =
        Document::PreviewSymbolFor(file, Path(), Document::ClipId{.sprite = kNamed});
    REQUIRE(preview.has_value());
    CHECK(preview->name == "Spinner");
    CHECK(preview->ifs == file.Encode().value_or(std::vector<uint8_t>{}));
}

TEST_CASE("An unexported sprite previews under a name added only to the preview bytes") {
    const Document::File file = Package(Scene());
    const auto before = file.Encode();
    REQUIRE(before.has_value());

    const auto preview =
        Document::PreviewSymbolFor(file, Path(), Document::ClipId{.sprite = kUnnamed});
    REQUIRE(preview.has_value());
    CHECK_FALSE(preview->name.empty());

    const AfpAnimation::Animation shown = Previewed(*preview);
    const auto added = std::ranges::find(shown.exports, kUnnamed, &AfpAnimation::Export::tag);
    REQUIRE(added != shown.exports.end());
    CHECK(Document::StringText(shown, added->name) == preview->name);
    CHECK(shown.exports.size() == 3);

    CHECK(file.Encode() == before);
}

TEST_CASE("A preview export keeps the table in the order the game searches it") {
    const Document::File file = Package(Scene());
    const auto preview =
        Document::PreviewSymbolFor(file, Path(), Document::ClipId{.sprite = kUnnamed});
    REQUIRE(preview.has_value());

    std::vector<std::string> folded;
    for (const std::string& name : ExportNames(Previewed(*preview)))
        folded.push_back(Folded(name));
    INFO(folded.front());
    CHECK(std::ranges::is_sorted(folded));
}

TEST_CASE("A preview name never matches an export the animation already has") {
    AfpAnimation::Animation animation = Scene();
    const Document::File plain = Package(animation);
    const auto first =
        Document::PreviewSymbolFor(plain, Path(), Document::ClipId{.sprite = kUnnamed});
    REQUIRE(first.has_value());

    animation.exports.push_back(
        AfpAnimation::Export{.tag = 41, .name = Document::InternString(animation, first->name)});
    std::ranges::sort(animation.exports, {}, [&animation](const AfpAnimation::Export& exported) {
        return Folded(Document::StringText(animation, exported.name));
    });
    const Document::File taken = Package(animation);
    const auto second =
        Document::PreviewSymbolFor(taken, Path(), Document::ClipId{.sprite = kUnnamed});
    REQUIRE(second.has_value());
    CHECK(Folded(second->name) != Folded(first->name));
    for (const std::string& name : ExportNames(animation))
        CHECK(Folded(name) != Folded(second->name));
}

TEST_CASE("The root and a sprite that is gone have no symbol to preview") {
    const Document::File file = Package(Scene());
    CHECK_FALSE(Document::PreviewSymbolFor(file, Path(), Document::ClipId{}).has_value());
    const auto gone =
        Document::PreviewSymbolFor(file, Path(), Document::ClipId{.sprite = uint16_t{77}});
    REQUIRE_FALSE(gone.has_value());
    CHECK(gone.error() == Document::MissingClipMessage(Document::ClipId{.sprite = uint16_t{77}}));
}
