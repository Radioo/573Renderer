#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/clip.h"
#include "document/clip_extract.h"
#include "document/frame_edit.h"
#include "document/label_edit.h"
#include "document/placement_effect.h"
#include "document/span_tags.h"
#include "document/tags.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kUseColour = 0x8;
constexpr uint16_t kShape = 1;
constexpr uint16_t kSprite = 3;
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
    return animation;
}

void Add(AfpAnimation::Animation& animation, uint16_t depth, uint16_t character, uint32_t first,
         uint32_t last) {
    const auto added = Document::AddDepth(animation, kRoot, depth, character, first, last);
    REQUIRE(added.has_value());
}

void MoveTo(AfpAnimation::Animation& animation, uint16_t depth, uint32_t frame, int32_t x) {
    AfpAnimation::Placement moved;
    moved.flags = kUpdateExisting | kUseMatrix;
    moved.depth = depth;
    moved.translation = std::array<int32_t, 2>{x, 0};
    Document::InsertTag(animation.root, frame, AfpAnimation::Tag{moved});
}

void TintAt(AfpAnimation::Animation& animation, uint16_t depth, uint32_t frame) {
    AfpAnimation::Placement tinted;
    tinted.flags = kUpdateExisting | kUseColour;
    tinted.depth = depth;
    tinted.multiply_colour = std::array<int16_t, 4>{128, 256, 256, 256};
    Document::InsertTag(animation.root, frame, AfpAnimation::Tag{tinted});
}

std::vector<Document::AppliedState> States(const AfpAnimation::Animation& animation, uint16_t depth,
                                           uint32_t first, uint32_t last) {
    std::vector<Document::AppliedState> states;
    for (const auto& [frame, state] : Document::ReplayDepth(animation.root, depth, first, last))
        states.push_back(state);
    return states;
}

std::map<uint16_t, std::vector<Document::Span>> Spans(const AfpAnimation::Animation& animation) {
    std::map<uint16_t, std::vector<Document::Span>> spans;
    for (const Document::DepthRow& row : Document::DepthRows(animation.root))
        spans[row.depth] = row.spans;
    return spans;
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

AfpAnimation::Animation Scene() {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, 1, kShape, 0, 9);
    MoveTo(animation, 1, 2, 200);
    MoveTo(animation, 1, 4, 400);
    TintAt(animation, 1, 5);
    MoveTo(animation, 1, 7, 700);
    Add(animation, 2, kShape, 3, 6);
    Add(animation, 3, kShape, 0, 4);
    MoveTo(animation, 3, 1, 100);
    Add(animation, 4, kShape, 5, 9);
    MoveTo(animation, 4, 6, 600);
    MoveTo(animation, 4, 8, 800);
    Add(animation, 5, kShape, 0, 2);
    return animation;
}

using Layout = std::map<uint16_t, std::vector<Document::Span>>;

}

TEST_CASE("Extracting frames closes the gap and keeps what every other frame shows") {
    AfpAnimation::Animation animation = Scene();
    const auto first_before = States(animation, 1, 0, 2);
    const auto first_after = States(animation, 1, 7, 9);
    const auto third_before = States(animation, 3, 0, 2);
    const auto fourth_after = States(animation, 4, 7, 9);
    const auto extracted = Document::ExtractFrames(
        animation, kRoot, Document::Span{.first_frame = 3, .last_frame = 6});
    INFO(Error(extracted));
    REQUIRE(extracted.has_value());
    CHECK(*extracted == 0);
    CHECK(animation.root.frames.size() == 6);
    CHECK(Spans(animation) == Layout{{1, {{0, 5}}}, {3, {{0, 2}}}, {4, {{3, 5}}}, {5, {{0, 2}}}});
    CHECK(States(animation, 1, 0, 2) == first_before);
    CHECK(States(animation, 1, 3, 5) == first_after);
    CHECK(States(animation, 3, 0, 2) == third_before);
    CHECK(States(animation, 4, 3, 5) == fourth_after);
    CHECK(std::ranges::none_of(animation.root.tags, [](const AfpAnimation::Tag& tag) {
        return Document::IsRemoveOf(tag, 2) || Document::IsPlacementOf(tag, 2);
    }));
}

TEST_CASE("Extracting frames carries the updates of the cut onto the frame after it") {
    AfpAnimation::Animation animation = Clip(8);
    Add(animation, 1, kShape, 0, 7);
    MoveTo(animation, 1, 2, 200);
    TintAt(animation, 1, 3);
    const auto kept = States(animation, 1, 0, 1);
    const auto after = States(animation, 1, 5, 7);
    REQUIRE(
        Document::ExtractFrames(animation, kRoot, Document::Span{.first_frame = 2, .last_frame = 4})
            .has_value());
    CHECK(States(animation, 1, 0, 1) == kept);
    CHECK(States(animation, 1, 2, 4) == after);
    CHECK(after.front().multiply[0] < 1.0);
    CHECK(after.front().matrix[4] == 200.0);
}

TEST_CASE("Extracting frames counts the sprites that cross the cut") {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, 1, kSprite, 0, 9);
    Add(animation, 2, kSprite, 5, 9);
    Add(animation, 3, kSprite, 0, 4);
    Add(animation, 4, kShape, 0, 9);
    Add(animation, 5, kSprite, 7, 9);
    const auto extracted = Document::ExtractFrames(
        animation, kRoot, Document::Span{.first_frame = 3, .last_frame = 6});
    INFO(Error(extracted));
    REQUIRE(extracted.has_value());
    CHECK(*extracted == 2);
}

TEST_CASE("Extracting frames keeps labels and refuses what it cannot cut") {
    AfpAnimation::Animation animation = Scene();
    REQUIRE(Document::AddLabel(animation, kRoot, "in", 4).has_value());
    REQUIRE(Document::AddLabel(animation, kRoot, "after", 8).has_value());
    const AfpAnimation::Animation before = animation;
    CHECK_FALSE(
        Document::ExtractFrames(animation, kRoot, Document::Span{.first_frame = 5, .last_frame = 4})
            .has_value());
    const auto outside = Document::ExtractFrames(
        animation, kRoot, Document::Span{.first_frame = 3, .last_frame = 10});
    REQUIRE_FALSE(outside.has_value());
    CHECK(outside.error().find("not frames of the clip") != std::string::npos);
    const auto everything = Document::ExtractFrames(
        animation, kRoot, Document::Span{.first_frame = 0, .last_frame = 9});
    REQUIRE_FALSE(everything.has_value());
    CHECK(everything.error().find("empty") != std::string::npos);
    CHECK(animation == before);
    REQUIRE(
        Document::ExtractFrames(animation, kRoot, Document::Span{.first_frame = 3, .last_frame = 6})
            .has_value());
    std::map<std::string, uint16_t> frames;
    for (const AfpAnimation::Label& label : animation.root.labels)
        frames[animation.strings.at(label.name)] = label.frame;
    CHECK(frames == std::map<std::string, uint16_t>{{"after", uint16_t{4}}, {"in", uint16_t{3}}});
}
