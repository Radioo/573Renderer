#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/clip.h"
#include "document/document.h"
#include "document/frame_edit.h"
#include "document/hidden_depths.h"
#include "document/tags.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "formats/ifs_archive.h"
#include "sample_package.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

const Document::ClipId kRoot{};
constexpr uint16_t kSprite = 40;

void AddLayers(AfpAnimation::Animation& animation) {
    AfpAnimation::Sprite sprite{.id = kSprite, .container = {}};
    sprite.container.frames.assign(3, AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{sprite});
    REQUIRE(Document::AddDepth(animation, kRoot, 61, 7, 0, 2).has_value());
    REQUIRE(Document::AddDepth(animation, kRoot, 62, 7, 1, 2).has_value());
    REQUIRE(Document::AddDepth(animation, kRoot, 63, kSprite, 0, 2).has_value());
    const Document::ClipId inside{.sprite = kSprite};
    REQUIRE(Document::AddDepth(animation, inside, 62, 7, 0, 1).has_value());
}

AfpAnimation::Animation Layered() {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    for (std::size_t i = 0; i < 3; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    AddLayers(animation);
    return animation;
}

bool Has(const std::vector<uint16_t>& depths, uint16_t depth) {
    return std::ranges::find(depths, depth) != depths.end();
}

std::vector<uint16_t> Depths(const AfpAnimation::Container& clip) {
    std::vector<uint16_t> depths;
    for (const Document::DepthRow& row : Document::DepthRows(clip))
        depths.push_back(row.depth);
    return depths;
}

const AfpAnimation::Container& SpriteClip(const AfpAnimation::Animation& animation) {
    const AfpAnimation::Container* found =
        Document::FindClip(animation, Document::ClipId{.sprite = kSprite});
    REQUIRE(found != nullptr);
    return *found;
}

}

TEST_CASE("Hiding depths drops every tag of those depths from that clip only") {
    AfpAnimation::Animation animation = Layered();
    REQUIRE(Document::HideDepths(animation, kRoot, {62, 9}).has_value());
    CHECK(Depths(animation.root) == std::vector<uint16_t>{61, 63});
    CHECK(Depths(SpriteClip(animation)) == std::vector<uint16_t>{62});
    CHECK(animation.root.frames.size() == 3);
    CHECK(animation.root.frames.back().first_tag + animation.root.frames.back().tag_count ==
          animation.root.tags.size());

    REQUIRE(Document::HideDepths(animation, Document::ClipId{.sprite = kSprite}, {62}).has_value());
    CHECK(Depths(SpriteClip(animation)).empty());
    CHECK(SpriteClip(animation).frames.size() == 3);

    CHECK_FALSE(
        Document::HideDepths(animation, Document::ClipId{.sprite = uint16_t{99}}, {1}).has_value());
}

TEST_CASE("A view without hidden depths leaves the document itself alone") {
    const auto bytes = Ifs::Write(SamplePackage::SampleArchive());
    REQUIRE(bytes.has_value());
    auto file = Document::File::Open(*bytes);
    REQUIRE(file.has_value());
    if (!file) return;
    const std::string path = "afp/" + SamplePackage::HashPath("intro");
    auto animation = file->ReadAnimation(path);
    REQUIRE(animation.has_value());
    if (!animation) return;
    AddLayers(*animation);
    const auto written = file->WriteAnimation(path, *animation);
    const std::string error = written.has_value() ? std::string() : written.error();
    INFO(error);
    REQUIRE(written.has_value());
    const auto before = file->Encode();

    const auto view = Document::ViewWithout(
        *file, {Document::DepthInClip{.animation = path, .clip = {}, .depth = 61},
                Document::DepthInClip{
                    .animation = path, .clip = Document::ClipId{.sprite = kSprite}, .depth = 62}});
    REQUIRE(view.has_value());
    if (!view) return;
    const auto shown = view->ReadAnimation(path);
    REQUIRE(shown.has_value());
    if (!shown) return;
    CHECK_FALSE(Has(Depths(shown->root), 61));
    CHECK(Has(Depths(shown->root), 62));
    CHECK(Has(Depths(shown->root), 63));
    CHECK(Depths(SpriteClip(*shown)).empty());
    CHECK(file->Encode() == before);

    CHECK_FALSE(
        Document::ViewWithout(
            *file, {Document::DepthInClip{.animation = "afp/missing", .clip = {}, .depth = 1}})
            .has_value());
}
