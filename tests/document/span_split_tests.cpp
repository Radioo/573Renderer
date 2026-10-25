#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/clip.h"
#include "document/frame_edit.h"
#include "document/placement_effect.h"
#include "document/span_split.h"
#include "document/tags.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint16_t kDepth = 2;
constexpr uint16_t kShape = 1;
constexpr uint16_t kSprite = 3;
constexpr uint16_t kImage = 4;
const Document::ClipId kRoot{};

AfpAnimation::Animation Clip(std::size_t frames) {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    for (std::size_t i = 0; i < frames; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    Document::InsertTag(animation.root, 0,
                        AfpAnimation::Tag{AfpAnimation::Shape{.unread_word = 0, .id = kShape}});
    Document::InsertTag(animation.root, 0,
                        AfpAnimation::Tag{AfpAnimation::Sprite{.id = kSprite, .container = {}}});
    Document::InsertTag(
        animation.root, 0,
        AfpAnimation::Tag{AfpAnimation::Image{.flags = 0, .id = kImage, .name = 0}});
    return animation;
}

void Add(AfpAnimation::Animation& animation, uint16_t character, uint32_t first, uint32_t last) {
    const auto added = Document::AddDepth(animation, kRoot, kDepth, character, first, last);
    REQUIRE(added.has_value());
}

void MoveTo(AfpAnimation::Animation& animation, uint32_t frame, int32_t x) {
    AfpAnimation::Placement moved;
    moved.flags = kUpdateExisting | kUseMatrix;
    moved.depth = kDepth;
    moved.translation = std::array<int32_t, 2>{x, 0};
    Document::InsertTag(animation.root, frame, AfpAnimation::Tag{moved});
}

Document::DepthRow RowOf(const AfpAnimation::Animation& animation) {
    for (const Document::DepthRow& row : Document::DepthRows(animation.root)) {
        if (row.depth == kDepth) return row;
    }
    return {};
}

auto Replayed(const AfpAnimation::Animation& animation) {
    return Document::ReplayDepth(animation.root, kDepth, 0,
                                 static_cast<uint32_t>(animation.root.frames.size() - 1));
}

std::size_t Removes(const AfpAnimation::Animation& animation) {
    return static_cast<std::size_t>(
        std::ranges::count_if(animation.root.tags, [](const AfpAnimation::Tag& tag) {
            const auto* remove = std::get_if<AfpAnimation::Remove>(&tag.body);
            return remove != nullptr && remove->depth == kDepth;
        }));
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

using Spans = std::vector<Document::Span>;

}

TEST_CASE("A span splits at a frame into two that replay exactly as the one did") {
    AfpAnimation::Animation animation = Clip(8);
    Add(animation, kShape, 0, 5);
    MoveTo(animation, 2, 200);
    MoveTo(animation, 4, 400);
    const auto before = Replayed(animation);
    const auto split = Document::SplitSpan(animation, kRoot, kDepth, 3);
    INFO(Error(split));
    REQUIRE(split.has_value());
    CHECK(RowOf(animation).spans == Spans{{0, 2}, {3, 5}});
    CHECK(RowOf(animation).shows == std::map<uint32_t, uint16_t>{{0, kShape}, {3, kShape}});
    CHECK(Replayed(animation) == before);
    CHECK(Removes(animation) == 2);
}

TEST_CASE("A span running to the end of the clip and one right before another split cleanly") {
    AfpAnimation::Animation animation = Clip(8);
    Add(animation, kShape, 0, 2);
    Add(animation, kShape, 3, 7);
    MoveTo(animation, 5, 300);
    MoveTo(animation, 6, 350);
    const auto before = Replayed(animation);
    REQUIRE(Document::SplitSpan(animation, kRoot, kDepth, 6).has_value());
    REQUIRE(Document::SplitSpan(animation, kRoot, kDepth, 1).has_value());
    CHECK(RowOf(animation).spans == Spans{{0, 0}, {1, 2}, {3, 5}, {6, 7}});
    CHECK(Replayed(animation) == before);
    CHECK(Removes(animation) == 3);
}

TEST_CASE("A split is refused, leaving the clip alone, where a new object would differ") {
    AfpAnimation::Animation animation = Clip(8);
    Add(animation, kShape, 0, 2);
    Add(animation, kSprite, 3, 5);
    const AfpAnimation::Animation before = animation;
    const auto first = Document::SplitSpan(animation, kRoot, kDepth, 0);
    REQUIRE_FALSE(first.has_value());
    CHECK(first.error().find("nothing before it") != std::string::npos);
    CHECK_FALSE(Document::SplitSpan(animation, kRoot, kDepth, 7).has_value());
    CHECK_FALSE(Document::SplitSpan(animation, kRoot, kDepth + 1, 1).has_value());
    const auto sprite = Document::SplitSpan(animation, kRoot, kDepth, 4);
    REQUIRE_FALSE(sprite.has_value());
    CHECK(sprite.error().find("start again") != std::string::npos);
    CHECK_FALSE(
        Document::SplitSpan(animation, Document::ClipId{.sprite = kSprite}, kDepth, 1).has_value());
    CHECK(animation == before);
}

TEST_CASE("An image splits as a shape does") {
    AfpAnimation::Animation animation = Clip(8);
    Add(animation, kImage, 1, 6);
    MoveTo(animation, 3, 250);
    const auto before = Replayed(animation);
    REQUIRE(Document::SplitSpan(animation, kRoot, kDepth, 4).has_value());
    CHECK(RowOf(animation).spans == Spans{{1, 3}, {4, 6}});
    CHECK(RowOf(animation).shows == std::map<uint32_t, uint16_t>{{1, kImage}, {4, kImage}});
    CHECK(Replayed(animation) == before);
}

TEST_CASE("A split looks at the character a swap shows on its frame") {
    AfpAnimation::Animation animation = Clip(8);
    Add(animation, kSprite, 0, 5);
    AfpAnimation::Placement swap;
    swap.flags = kUpdateExisting;
    swap.depth = kDepth;
    swap.character = kShape;
    Document::InsertTag(animation.root, 2, AfpAnimation::Tag{swap});
    const AfpAnimation::Animation before = animation;
    CHECK_FALSE(Document::SplitSpan(animation, kRoot, kDepth, 1).has_value());
    CHECK(animation == before);
    const auto split = Document::SplitSpan(animation, kRoot, kDepth, 3);
    INFO(Error(split));
    REQUIRE(split.has_value());
    CHECK(RowOf(animation).spans == Spans{{0, 2}, {3, 5}});
    CHECK(RowOf(animation).shows.at(3) == kShape);
}
