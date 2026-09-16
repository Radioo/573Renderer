#include <catch2/catch_test_macros.hpp>

#include "document/frame_edit.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace {

AfpAnimation::Animation Clip(std::size_t frames) {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    for (std::size_t i = 0; i < frames; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    return animation;
}

std::vector<Document::Span> SpansAt(const AfpAnimation::Animation& animation, uint16_t depth) {
    for (const Document::DepthRow& row : Document::DepthRows(animation.root)) {
        if (row.depth == depth) return row.spans;
    }
    return {};
}

bool FramesAreConsistent(const AfpAnimation::Container& clip) {
    std::size_t expected = clip.frames.empty() ? 0 : clip.frames.front().first_tag;
    for (const AfpAnimation::Frame& frame : clip.frames) {
        if (frame.first_tag != expected) return false;
        expected += frame.tag_count;
    }
    return expected == clip.tags.size();
}

}

TEST_CASE("A depth added over a range places and removes on the right frames") {
    AfpAnimation::Animation animation = Clip(6);
    REQUIRE(Document::AddDepth(animation, 4, 12, 1, 3).has_value());
    CHECK(FramesAreConsistent(animation.root));

    const std::vector<Document::Span> spans = SpansAt(animation, 4);
    REQUIRE(spans.size() == 1);
    CHECK(spans[0].first_frame == 1);
    CHECK(spans[0].last_frame == 3);

    REQUIRE(animation.root.frames[1].tag_count == 1);
    REQUIRE(animation.root.frames[4].tag_count == 1);
    const auto* placed = std::get_if<AfpAnimation::Placement>(
        &animation.root.tags[animation.root.frames[1].first_tag].body);
    REQUIRE(placed != nullptr);
    CHECK(placed->depth == 4);
    CHECK(placed->end_frame == 4);
    REQUIRE(placed->character.has_value());
    CHECK(*placed->character == 12);
    CHECK((placed->flags & 0x1) == 0);
    const auto* removed = std::get_if<AfpAnimation::Remove>(
        &animation.root.tags[animation.root.frames[4].first_tag].body);
    REQUIRE(removed != nullptr);
    CHECK(removed->depth == 4);
}

TEST_CASE("A depth that runs to the last frame needs no remove") {
    AfpAnimation::Animation animation = Clip(3);
    REQUIRE(Document::AddDepth(animation, 2, 1, 0, 2).has_value());
    CHECK(animation.root.tags.size() == 1);
    const std::vector<Document::Span> spans = SpansAt(animation, 2);
    REQUIRE(spans.size() == 1);
    CHECK(spans[0].last_frame == 2);
}

TEST_CASE("A depth the clip cannot take is refused") {
    AfpAnimation::Animation animation = Clip(4);
    REQUIRE(Document::AddDepth(animation, 1, 1, 1, 2).has_value());
    CHECK_FALSE(Document::AddDepth(animation, 1, 1, 2, 3).has_value());
    CHECK_FALSE(Document::AddDepth(animation, 1, 1, 3, 1).has_value());
    CHECK_FALSE(Document::AddDepth(animation, 1, 1, 0, 9).has_value());
    CHECK(animation.root.tags.size() == 2);
    CHECK(FramesAreConsistent(animation.root));
}

TEST_CASE("Removing a depth takes its placement and its remove away") {
    AfpAnimation::Animation animation = Clip(6);
    REQUIRE(Document::AddDepth(animation, 4, 12, 1, 3).has_value());
    REQUIRE(Document::AddDepth(animation, 5, 13, 0, 5).has_value());
    REQUIRE(Document::RemoveDepth(animation, 4, 2).has_value());
    CHECK(FramesAreConsistent(animation.root));
    CHECK(SpansAt(animation, 4).empty());
    REQUIRE(SpansAt(animation, 5).size() == 1);
    CHECK(SpansAt(animation, 5)[0].last_frame == 5);
    CHECK_FALSE(Document::RemoveDepth(animation, 4, 2).has_value());
    CHECK_FALSE(Document::RemoveDepth(animation, 5, 9).has_value());
}

TEST_CASE("An inserted frame moves the tags, labels and end frames after it") {
    AfpAnimation::Animation animation = Clip(4);
    REQUIRE(Document::AddDepth(animation, 3, 7, 2, 3).has_value());
    animation.strings.emplace_back("late");
    animation.root.labels.push_back(AfpAnimation::Label{.frame = 2, .name = 1});

    REQUIRE(Document::InsertFrame(animation, 1).has_value());
    CHECK(animation.root.frames.size() == 5);
    CHECK(FramesAreConsistent(animation.root));
    CHECK(animation.root.labels.front().frame == 3);

    const std::vector<Document::Span> spans = SpansAt(animation, 3);
    REQUIRE(spans.size() == 1);
    CHECK(spans[0].first_frame == 3);
    CHECK(spans[0].last_frame == 4);
    const auto* placed = std::get_if<AfpAnimation::Placement>(
        &animation.root.tags[animation.root.frames[3].first_tag].body);
    REQUIRE(placed != nullptr);
    CHECK(placed->end_frame == 5);
}

TEST_CASE("A frame appended at the end takes the tags that follow") {
    AfpAnimation::Animation animation = Clip(2);
    REQUIRE(Document::AddDepth(animation, 1, 1, 0, 1).has_value());
    REQUIRE(Document::InsertFrame(animation, 2).has_value());
    CHECK(animation.root.frames.size() == 3);
    CHECK(FramesAreConsistent(animation.root));
    CHECK_FALSE(Document::InsertFrame(animation, 4).has_value());
}

TEST_CASE("A removed frame takes its tags and moves what follows back") {
    AfpAnimation::Animation animation = Clip(5);
    REQUIRE(Document::AddDepth(animation, 3, 7, 1, 2).has_value());
    animation.strings.emplace_back("here");
    animation.root.labels.push_back(AfpAnimation::Label{.frame = 4, .name = 1});

    REQUIRE(Document::RemoveFrame(animation, 0).has_value());
    CHECK(animation.root.frames.size() == 4);
    CHECK(FramesAreConsistent(animation.root));
    CHECK(animation.root.labels.front().frame == 3);
    const std::vector<Document::Span> spans = SpansAt(animation, 3);
    REQUIRE(spans.size() == 1);
    CHECK(spans[0].first_frame == 0);
    CHECK(spans[0].last_frame == 1);
}

TEST_CASE("Removing the frame a label sits on keeps the label in the clip") {
    AfpAnimation::Animation animation = Clip(3);
    animation.strings.emplace_back("last");
    animation.root.labels.push_back(AfpAnimation::Label{.frame = 2, .name = 1});
    REQUIRE(Document::RemoveFrame(animation, 2).has_value());
    CHECK(animation.root.labels.front().frame == 1);
    REQUIRE(Document::RemoveFrame(animation, 0).has_value());
    CHECK(animation.root.labels.front().frame == 0);
    CHECK_FALSE(Document::RemoveFrame(animation, 0).has_value());
}

TEST_CASE("Removing a frame drops the tags that were on it") {
    AfpAnimation::Animation animation = Clip(4);
    REQUIRE(Document::AddDepth(animation, 2, 5, 1, 2).has_value());
    REQUIRE(animation.root.tags.size() == 2);
    REQUIRE(Document::RemoveFrame(animation, 1).has_value());
    CHECK(animation.root.tags.size() == 1);
    CHECK(FramesAreConsistent(animation.root));
    CHECK(SpansAt(animation, 2).empty());
}
