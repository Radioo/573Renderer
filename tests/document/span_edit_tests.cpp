#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/clip.h"
#include "document/frame_edit.h"
#include "document/keyframes.h"
#include "document/span_edit.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

constexpr uint16_t kDepth = 2;
const Document::ClipId kRoot{};

AfpAnimation::Animation Clip(std::size_t frames) {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    for (std::size_t i = 0; i < frames; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    return animation;
}

void Add(AfpAnimation::Animation& animation, uint16_t depth, uint32_t first, uint32_t last) {
    const auto added = Document::AddDepth(animation, kRoot, depth, 7, first, last);
    REQUIRE(added.has_value());
}

std::vector<Document::Span> SpansOf(const AfpAnimation::Animation& animation, uint16_t depth) {
    for (const Document::DepthRow& row : Document::DepthRows(animation.root)) {
        if (row.depth == depth) return row.spans;
    }
    return {};
}

std::vector<uint16_t> EndFrames(const AfpAnimation::Animation& animation, uint16_t depth) {
    std::vector<uint16_t> ends;
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement != nullptr && placement->depth == depth) ends.push_back(placement->end_frame);
    }
    return ends;
}

std::size_t Removes(const AfpAnimation::Animation& animation, uint16_t depth) {
    return static_cast<std::size_t>(
        std::ranges::count_if(animation.root.tags, [depth](const AfpAnimation::Tag& tag) {
            const auto* remove = std::get_if<AfpAnimation::Remove>(&tag.body);
            return remove != nullptr && remove->depth == depth;
        }));
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

}

TEST_CASE("A span moves later with its end frame and its remove") {
    AfpAnimation::Animation animation = Clip(8);
    Add(animation, kDepth, 1, 3);
    const auto moved = Document::MoveSpan(animation, kRoot, kDepth, 2, 2);
    INFO(Error(moved));
    REQUIRE(moved.has_value());
    CHECK(*moved == Document::Span{.first_frame = 3, .last_frame = 5});
    CHECK(SpansOf(animation, kDepth) == std::vector<Document::Span>{{3, 5}});
    CHECK(EndFrames(animation, kDepth) == std::vector<uint16_t>{6});
    CHECK(Removes(animation, kDepth) == 1);
}

TEST_CASE("A span moved onto the last frame loses its remove and one moved back gains one") {
    AfpAnimation::Animation animation = Clip(8);
    Add(animation, kDepth, 1, 3);
    REQUIRE(Document::MoveSpan(animation, kRoot, kDepth, 1, 4).has_value());
    CHECK(SpansOf(animation, kDepth) == std::vector<Document::Span>{{5, 7}});
    CHECK(Removes(animation, kDepth) == 0);
    REQUIRE(Document::MoveSpan(animation, kRoot, kDepth, 7, -5).has_value());
    CHECK(SpansOf(animation, kDepth) == std::vector<Document::Span>{{0, 2}});
    CHECK(Removes(animation, kDepth) == 1);
}

TEST_CASE("A span can move up to another span of its depth but not into it") {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, kDepth, 0, 2);
    Add(animation, kDepth, 6, 8);
    REQUIRE(Document::MoveSpan(animation, kRoot, kDepth, 1, 3).has_value());
    CHECK(SpansOf(animation, kDepth) == std::vector<Document::Span>{{3, 5}, {6, 8}});
    const AfpAnimation::Animation before = animation;
    CHECK_FALSE(Document::MoveSpan(animation, kRoot, kDepth, 7, -1).has_value());
    CHECK_FALSE(Document::MoveSpan(animation, kRoot, kDepth, 3, 3).has_value());
    CHECK_FALSE(Document::MoveSpan(animation, kRoot, kDepth, 3, -4).has_value());
    CHECK_FALSE(Document::MoveSpan(animation, kRoot, kDepth, 9, 1).has_value());
    CHECK_FALSE(Document::MoveSpan(animation, Document::ClipId{.sprite = uint16_t{3}}, kDepth, 3, 1)
                    .has_value());
    CHECK(animation == before);
}

TEST_CASE("A span next to another keeps the order a remove and a create need") {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, kDepth, 0, 2);
    Add(animation, kDepth, 5, 7);
    REQUIRE(Document::MoveSpan(animation, kRoot, kDepth, 1, 2).has_value());
    CHECK(SpansOf(animation, kDepth) == std::vector<Document::Span>{{2, 4}, {5, 7}});
    CHECK_FALSE(Document::MoveSpan(animation, kRoot, kDepth, 2, 1).has_value());
    REQUIRE(Document::MoveSpan(animation, kRoot, kDepth, 6, 2).has_value());
    CHECK(SpansOf(animation, kDepth) == std::vector<Document::Span>{{2, 4}, {7, 9}});
    REQUIRE(Document::MoveSpan(animation, kRoot, kDepth, 8, -2).has_value());
    CHECK(SpansOf(animation, kDepth) == std::vector<Document::Span>{{2, 4}, {5, 7}});
}

TEST_CASE("A span moves to a depth that is free over its frames") {
    AfpAnimation::Animation animation = Clip(8);
    Add(animation, kDepth, 1, 3);
    Add(animation, 5, 4, 6);
    REQUIRE(Document::ChangeSpanDepth(animation, kRoot, kDepth, 2, 9).has_value());
    CHECK(SpansOf(animation, kDepth).empty());
    CHECK(SpansOf(animation, 9) == std::vector<Document::Span>{{1, 3}});
    CHECK(Removes(animation, 9) == 1);

    const AfpAnimation::Animation before = animation;
    CHECK_FALSE(Document::ChangeSpanDepth(animation, kRoot, 9, 2, 5).has_value());
    CHECK_FALSE(Document::ChangeSpanDepth(animation, kRoot, 9, 2, 0x3000).has_value());
    CHECK_FALSE(Document::ChangeSpanDepth(animation, kRoot, 9, 6, 1).has_value());
    CHECK(animation == before);
    CHECK(Document::ChangeSpanDepth(animation, kRoot, 9, 2, 9).has_value());
}

TEST_CASE("Restacking a span leaves the remove of the span before it alone") {
    AfpAnimation::Animation animation = Clip(8);
    Add(animation, kDepth, 0, 2);
    Add(animation, kDepth, 3, 5);
    REQUIRE(Document::ChangeSpanDepth(animation, kRoot, kDepth, 4, 9).has_value());
    CHECK(SpansOf(animation, kDepth) == std::vector<Document::Span>{{0, 2}});
    CHECK(SpansOf(animation, 9) == std::vector<Document::Span>{{3, 5}});
    CHECK(Removes(animation, kDepth) == 1);
    CHECK(Removes(animation, 9) == 1);
}

TEST_CASE("An owned depth's range and keyframes follow its span") {
    Document::AuthoredDepth authored{
        .animation = "afp/a",
        .depth = kDepth,
        .first_frame = 1,
        .last_frame = 3,
        .tracks = {Document::Track{
            .property = "Translation",
            .keys = {Document::Keyframe{
                         .frame = 1, .value = {0, 0}, .ease = Document::Ease::Hold, .bezier = {}},
                     Document::Keyframe{.frame = 3,
                                        .value = {20, 0},
                                        .ease = Document::Ease::Hold,
                                        .bezier = {}}}}},
        .script = std::nullopt,
        .clip = {}};
    REQUIRE(Document::ShiftAuthored(authored, 2).has_value());
    CHECK(authored.first_frame == 3);
    CHECK(authored.last_frame == 5);
    CHECK(authored.tracks[0].keys[0].frame == 3);
    CHECK(authored.tracks[0].keys[1].frame == 5);
    CHECK_FALSE(Document::ShiftAuthored(authored, -4).has_value());
    CHECK(authored.first_frame == 3);
}
