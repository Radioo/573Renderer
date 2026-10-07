#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/clip.h"
#include "document/clip_trim.h"
#include "document/frame_edit.h"
#include "document/label_edit.h"
#include "document/placement_effect.h"
#include "document/tags.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
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

std::vector<Document::AppliedState> States(const AfpAnimation::Animation& animation, uint16_t depth,
                                           uint32_t first, uint32_t last) {
    std::vector<Document::AppliedState> states;
    for (const auto& [frame, state] : Document::ReplayDepth(animation.root, depth, first, last))
        states.push_back(state);
    return states;
}

std::vector<uint16_t> DepthsOf(const AfpAnimation::Animation& animation) {
    std::vector<uint16_t> depths;
    for (const Document::DepthRow& row : Document::DepthRows(animation.root))
        depths.push_back(row.depth);
    return depths;
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

AfpAnimation::Animation Scene() {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, 1, kShape, 0, 9);
    MoveTo(animation, 1, 2, 200);
    MoveTo(animation, 1, 5, 500);
    MoveTo(animation, 1, 8, 800);
    Add(animation, 2, kShape, 3, 6);
    MoveTo(animation, 2, 4, 40);
    Add(animation, 3, kShape, 0, 1);
    Add(animation, 4, kShape, 8, 9);
    return animation;
}

}

TEST_CASE("Trimming a clip to a stretch keeps what every kept frame shows") {
    AfpAnimation::Animation animation = Scene();
    const auto first_depth = States(animation, 1, 3, 7);
    const auto second_depth = States(animation, 2, 3, 7);
    const auto trimmed = Document::TrimClipToFrames(
        animation, kRoot, Document::Span{.first_frame = 3, .last_frame = 7});
    INFO(Error(trimmed));
    REQUIRE(trimmed.has_value());
    CHECK(*trimmed == 0);
    CHECK(animation.root.frames.size() == 5);
    CHECK(DepthsOf(animation) == std::vector<uint16_t>{1, 2});
    CHECK(States(animation, 1, 0, 4) == first_depth);
    CHECK(States(animation, 2, 0, 4) == second_depth);
    CHECK(Document::DepthRows(animation.root).at(1).spans ==
          std::vector<Document::Span>{{.first_frame = 0, .last_frame = 3}});
}

TEST_CASE("Trimming a clip keeps its labels on the frames that stay") {
    AfpAnimation::Animation animation = Scene();
    REQUIRE(Document::AddLabel(animation, kRoot, "intro", 1).has_value());
    REQUIRE(Document::AddLabel(animation, kRoot, "loop", 5).has_value());
    REQUIRE(Document::TrimClipToFrames(animation, kRoot,
                                       Document::Span{.first_frame = 3, .last_frame = 7})
                .has_value());
    std::vector<uint16_t> frames;
    frames.reserve(animation.root.labels.size());
    for (const AfpAnimation::Label& label : animation.root.labels)
        frames.push_back(label.frame);
    CHECK(frames == std::vector<uint16_t>{0, 2});
}

TEST_CASE("Trimming a clip counts the sprites that start again on its new first frame") {
    AfpAnimation::Animation animation = Clip(8);
    Add(animation, 1, kSprite, 0, 7);
    Add(animation, 2, kSprite, 4, 7);
    Add(animation, 3, kShape, 0, 7);
    Add(animation, 4, kSprite, 2, 7);
    const auto trimmed = Document::TrimClipToFrames(
        animation, kRoot, Document::Span{.first_frame = 2, .last_frame = 6});
    INFO(Error(trimmed));
    REQUIRE(trimmed.has_value());
    CHECK(*trimmed == 1);
}

TEST_CASE("Trimming a clip is refused, leaving it alone, for frames it cannot keep") {
    AfpAnimation::Animation animation = Scene();
    const AfpAnimation::Animation before = animation;
    CHECK_FALSE(Document::TrimClipToFrames(animation, kRoot,
                                           Document::Span{.first_frame = 5, .last_frame = 4})
                    .has_value());
    CHECK_FALSE(Document::TrimClipToFrames(animation, kRoot,
                                           Document::Span{.first_frame = 2, .last_frame = 10})
                    .has_value());
    CHECK_FALSE(Document::TrimClipToFrames(animation, kRoot,
                                           Document::Span{.first_frame = 0, .last_frame = 9})
                    .has_value());
    CHECK_FALSE(Document::TrimClipToFrames(animation, Document::ClipId{.sprite = kSprite},
                                           Document::Span{.first_frame = 0, .last_frame = 0})
                    .has_value());
    CHECK(animation == before);
}
