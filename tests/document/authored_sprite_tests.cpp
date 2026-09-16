#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/clip.h"
#include "document/keyframe_edit.h"
#include "document/keyframes.h"
#include "document/project.h"
#include "document/tags.h"
#include "formats/afp_animation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint16_t kDepth = 3;
constexpr uint16_t kSprite = 5;
const Document::ClipId kInSprite{.sprite = kSprite};
const Document::ClipId kRoot{};

AfpAnimation::Container Frames(std::size_t count) {
    AfpAnimation::Container clip;
    for (std::size_t i = 0; i < count; i++)
        clip.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    return clip;
}

AfpAnimation::Placement Create(int32_t x) {
    AfpAnimation::Placement placement;
    placement.depth = kDepth;
    placement.end_frame = 4;
    placement.character = uint16_t{7};
    placement.translation = std::array<int32_t, 2>{x, 0};
    return placement;
}

AfpAnimation::Placement Update(int32_t x) {
    AfpAnimation::Placement placement;
    placement.flags = kUpdateExisting;
    placement.depth = kDepth;
    placement.end_frame = 4;
    placement.translation = std::array<int32_t, 2>{x, 0};
    return placement;
}

void Move(AfpAnimation::Container& clip, int32_t from) {
    Document::InsertTag(clip, 0, AfpAnimation::Tag{Create(from)});
    for (uint32_t frame = 1; frame < 4; frame++) {
        Document::InsertTag(clip, frame,
                            AfpAnimation::Tag{Update(from + (static_cast<int32_t>(frame) * 10))});
    }
}

AfpAnimation::Animation Scene() {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    animation.root = Frames(5);
    Move(animation.root, 100);
    AfpAnimation::Container sprite = Frames(5);
    Move(sprite, 500);
    animation.root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Sprite{.id = kSprite, .container = sprite}});
    animation.root.frames.back().tag_count++;
    return animation;
}

const AfpAnimation::Container& ClipOf(const AfpAnimation::Animation& animation,
                                      Document::ClipId clip) {
    const AfpAnimation::Container* found = Document::FindClip(animation, clip);
    REQUIRE(found != nullptr);
    return *found;
}

std::optional<int32_t> XAt(const AfpAnimation::Container& clip, uint32_t frame) {
    const AfpAnimation::Frame& owner = clip.frames[frame];
    for (uint32_t i = 0; i < owner.tag_count; i++) {
        const auto* placement =
            std::get_if<AfpAnimation::Placement>(&clip.tags[owner.first_tag + i].body);
        if (placement != nullptr && placement->depth == kDepth && placement->translation)
            return (*placement->translation)[0];
    }
    return std::nullopt;
}

}

TEST_CASE("Owning a sprite depth captures the sprite's placements, not the root's") {
    const AfpAnimation::Animation animation = Scene();
    const auto owned = Document::OwnDepth(animation, kInSprite, "afp/a", kDepth, 1);
    REQUIRE(owned.has_value());
    CHECK(owned->authored.clip == kInSprite);
    const auto key = Document::KeyAt(owned->authored, "Translation", 2);
    REQUIRE(key.has_value());
    if (!key) return;
    CHECK(key->value == std::vector<int64_t>{520, 0});
}

TEST_CASE("Owning a root depth records the root") {
    const AfpAnimation::Animation animation = Scene();
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 1);
    REQUIRE(owned.has_value());
    CHECK(owned->authored.clip == kRoot);
    const auto key = Document::KeyAt(owned->authored, "Translation", 2);
    REQUIRE(key.has_value());
    if (!key) return;
    CHECK(key->value == std::vector<int64_t>{120, 0});
}

TEST_CASE("A sprite depth owned and written back with no edit leaves the animation as it was") {
    AfpAnimation::Animation animation = Scene();
    const AfpAnimation::Animation before = animation;
    const auto owned = Document::OwnDepth(animation, kInSprite, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    const auto baked = Document::BakedFor(animation, owned->authored);
    REQUIRE(baked.has_value());
    REQUIRE(Document::WriteAuthored(animation, owned->authored, *baked).has_value());
    CHECK(animation == before);
}

TEST_CASE("An edited sprite keyframe is written into the sprite and nowhere else") {
    AfpAnimation::Animation animation = Scene();
    auto owned = Document::OwnDepth(animation, kInSprite, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    REQUIRE(Document::SetKeyValueAt(owned->authored, "Translation", 2, "999, 0").has_value());
    const auto baked = Document::BakedFor(animation, owned->authored);
    REQUIRE(baked.has_value());
    REQUIRE(Document::WriteAuthored(animation, owned->authored, *baked).has_value());

    CHECK(XAt(ClipOf(animation, kInSprite), 2) == 999);
    CHECK(XAt(ClipOf(animation, kRoot), 2) == 120);
}

TEST_CASE("Authored content for a sprite that is gone is refused") {
    AfpAnimation::Animation animation = Scene();
    const Document::ClipId gone{.sprite = uint16_t{77}};
    CHECK_FALSE(Document::OwnDepth(animation, gone, "afp/a", kDepth, 0).has_value());

    auto owned = Document::OwnDepth(animation, kInSprite, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    owned->authored.clip = gone;
    const auto baked = Document::BakedFor(animation, owned->authored);
    REQUIRE_FALSE(baked.has_value());
    CHECK(baked.error() == Document::MissingClipMessage(gone));
    const AfpAnimation::Animation before = animation;
    CHECK_FALSE(Document::WriteAuthored(animation, owned->authored, owned->baked).has_value());
    CHECK(animation == before);
}

TEST_CASE("The manifest records the sprite an owned depth belongs to") {
    const AfpAnimation::Animation animation = Scene();
    const auto in_sprite = Document::OwnDepth(animation, kInSprite, "afp/a", kDepth, 0);
    const auto in_root = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(in_sprite.has_value());
    REQUIRE(in_root.has_value());
    const Document::Project project{.build = "iidx33",
                                    .ifs_path = "a.ifs",
                                    .content = {in_root->authored, in_sprite->authored},
                                    .images = {},
                                    .exported = {}};

    const std::vector<uint8_t> manifest = Document::WriteProject(project);
    const std::string text(manifest.begin(), manifest.end());
    CHECK(text.find("\"sprite\": 5") != std::string::npos);

    const auto read = Document::ReadProject(manifest);
    REQUIRE(read.has_value());
    REQUIRE(read->content.size() == 2);
    CHECK(read->content[0].clip == kRoot);
    CHECK(read->content[1].clip == kInSprite);
}

TEST_CASE("A manifest written before sprites existed reads as owning root depths") {
    const std::string text =
        R"({"format":1,"build":"b","ifs":"a.ifs","owns":[{"animation":"afp/a","depth":3,)"
        R"("first":0,"last":1,"tracks":[{"property":"Translation","keys":[)"
        R"({"frame":0,"value":[1,2],"ease":"hold"}]}]}]})";
    const auto read = Document::ReadProject(std::vector<uint8_t>(text.begin(), text.end()));
    REQUIRE(read.has_value());
    REQUIRE(read->content.size() == 1);
    CHECK(read->content.front().clip == kRoot);
}

TEST_CASE("A manifest sprite that is not a sprite number is refused") {
    const std::string head =
        R"({"format":1,"build":"b","ifs":"a.ifs","owns":[{"animation":"afp/a","depth":3,)"
        R"("first":0,"last":1,"tracks":[],"sprite":)";
    const auto read = [&head](const std::string& sprite) {
        const std::string text = head + sprite + "}]}";
        return Document::ReadProject(std::vector<uint8_t>(text.begin(), text.end()));
    };
    CHECK(read("5").has_value());
    CHECK_FALSE(read("\"5\"").has_value());
    CHECK_FALSE(read("-1").has_value());
    CHECK_FALSE(read("70000").has_value());
}
