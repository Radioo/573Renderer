#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/clip.h"
#include "document/frame_edit.h"
#include "document/span_clipboard.h"
#include "document/tags.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace {

constexpr uint16_t kCharacter = 7;
constexpr uint16_t kSprite = 30;
constexpr uint16_t kInner = 31;
const Document::ClipId kRoot{};
const Document::ClipId kInside{.sprite = kSprite};
const std::string kPath = "afp/a";

AfpAnimation::Sprite EmptySprite(uint16_t id, std::size_t frames) {
    AfpAnimation::Sprite sprite{.id = id, .container = {}};
    sprite.container.frames.assign(frames, AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    return sprite;
}

AfpAnimation::Animation Scene() {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    animation.root.frames.assign(12, AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{EmptySprite(kSprite, 8)});
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{EmptySprite(kInner, 4)});
    REQUIRE(Document::AddDepth(animation, kRoot, 2, kCharacter, 3, 6).has_value());
    AfpAnimation::Placement moved;
    moved.flags = 0x5;
    moved.depth = 2;
    moved.end_frame = 7;
    moved.translation = std::array<int32_t, 2>{80, 0};
    Document::InsertTag(animation.root, 5, AfpAnimation::Tag{moved});
    REQUIRE(Document::AddDepth(animation, kRoot, 4, kSprite, 0, 9).has_value());
    REQUIRE(Document::AddDepth(animation, kInside, 1, kInner, 0, 3).has_value());
    return animation;
}

std::vector<Document::Span> SpansOf(const AfpAnimation::Container& clip, uint16_t depth) {
    for (const Document::DepthRow& row : Document::DepthRows(clip)) {
        if (row.depth == depth) return row.spans;
    }
    return {};
}

const AfpAnimation::Container& Inside(const AfpAnimation::Animation& animation) {
    const AfpAnimation::Container* found = Document::FindClip(animation, kInside);
    REQUIRE(found != nullptr);
    return *found;
}

std::vector<AfpAnimation::Placement> PlacementsAt(const AfpAnimation::Container& clip,
                                                  uint16_t depth) {
    std::vector<AfpAnimation::Placement> found;
    for (const AfpAnimation::Tag& tag : clip.tags) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement != nullptr && placement->depth == depth) found.push_back(*placement);
    }
    return found;
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

}

TEST_CASE("A copied span pastes into another clip at the playhead on a free depth") {
    AfpAnimation::Animation animation = Scene();
    const auto copied = Document::CopySpan(animation, kPath, kRoot, 2, 4);
    INFO(Error(copied));
    REQUIRE(copied.has_value());
    if (!copied) return;
    CHECK(copied->length == 4);

    const auto pasted = Document::PasteSpan(animation, kPath, kInside, *copied, 5, 2);
    INFO(Error(pasted));
    REQUIRE(pasted.has_value());
    CHECK(*pasted == Document::Span{.first_frame = 2, .last_frame = 5});
    CHECK(SpansOf(Inside(animation), 5) == std::vector<Document::Span>{{2, 5}});
    const std::vector<AfpAnimation::Placement> placed = PlacementsAt(Inside(animation), 5);
    REQUIRE(placed.size() == 2);
    CHECK(placed[0].character == kCharacter);
    CHECK(placed[0].end_frame == 6);
    CHECK(placed[1].translation == std::array<int32_t, 2>{80, 0});
    CHECK(placed[1].end_frame == 6);
    CHECK(SpansOf(animation.root, 2) == std::vector<Document::Span>{{3, 6}});
}

TEST_CASE("A span pasted at the end of a clip runs to its last frame") {
    AfpAnimation::Animation animation = Scene();
    const auto copied = Document::CopySpan(animation, kPath, kRoot, 2, 3);
    REQUIRE(copied.has_value());
    if (!copied) return;
    const auto pasted = Document::PasteSpan(animation, kPath, kRoot, *copied, 8, 8);
    INFO(Error(pasted));
    REQUIRE(pasted.has_value());
    CHECK(SpansOf(animation.root, 8) == std::vector<Document::Span>{{8, 11}});
}

TEST_CASE("A paste that does not fit, overlaps or crosses animations is refused") {
    AfpAnimation::Animation animation = Scene();
    const auto copied = Document::CopySpan(animation, kPath, kRoot, 2, 3);
    REQUIRE(copied.has_value());
    if (!copied) return;
    const AfpAnimation::Animation before = animation;
    CHECK_FALSE(Document::PasteSpan(animation, kPath, kRoot, *copied, 8, 9).has_value());
    CHECK_FALSE(Document::PasteSpan(animation, kPath, kRoot, *copied, 4, 2).has_value());
    CHECK_FALSE(Document::PasteSpan(animation, kPath, kInside, *copied, 1, 0).has_value());
    CHECK_FALSE(Document::PasteSpan(animation, "afp/b", kRoot, *copied, 8, 0).has_value());
    CHECK(animation == before);
    CHECK_FALSE(Document::CopySpan(animation, kPath, kRoot, 2, 0).has_value());
}

TEST_CASE("A sprite is never pasted into a clip it contains") {
    AfpAnimation::Animation animation = Scene();
    const auto sprite = Document::CopySpan(animation, kPath, kRoot, 4, 0);
    REQUIRE(sprite.has_value());
    if (!sprite) return;
    const auto pasted = Document::PasteSpan(animation, kPath, kInside, *sprite, 6, 0);
    REQUIRE_FALSE(pasted.has_value());
    CHECK(pasted.error().find("itself") != std::string::npos);

    const auto inner = Document::CopySpan(animation, kPath, kInside, 1, 0);
    REQUIRE(inner.has_value());
    if (!inner) return;
    CHECK(Document::PasteSpan(animation, kPath, kRoot, *inner, 9, 0).has_value());
    const Document::ClipId in_inner{.sprite = kInner};
    CHECK_FALSE(Document::PasteSpan(animation, kPath, in_inner, *inner, 2, 0).has_value());
}
