#include <catch2/catch_test_macros.hpp>

#include "document/timeline.h"
#include "formats/afp_animation.h"

#include <cstdint>
#include <vector>

namespace {

constexpr uint32_t kCreate = 0x2;
constexpr uint32_t kUpdate = 0x3;

AfpAnimation::Tag Place(uint32_t flags, uint16_t depth) {
    AfpAnimation::Placement placement;
    placement.flags = flags;
    placement.depth = depth;
    return AfpAnimation::Tag{placement};
}

AfpAnimation::Tag Remove(uint16_t depth) {
    return AfpAnimation::Tag{AfpAnimation::Remove{.unread_word = 0, .depth = depth}};
}

AfpAnimation::Container ClipOf(const std::vector<std::vector<AfpAnimation::Tag>>& per_frame) {
    AfpAnimation::Container clip;
    for (const std::vector<AfpAnimation::Tag>& tags : per_frame) {
        clip.frames.push_back(
            AfpAnimation::Frame{.first_tag = static_cast<uint32_t>(clip.tags.size()),
                                .tag_count = static_cast<uint32_t>(tags.size())});
        for (const AfpAnimation::Tag& tag : tags) {
            clip.tags.push_back(tag);
        }
    }
    return clip;
}

}

TEST_CASE("A depth is placed from its create to its remove") {
    const AfpAnimation::Container clip = ClipOf({
        {Place(kCreate, 4)},
        {Place(kUpdate, 4)},
        {Place(kUpdate, 4)},
        {Remove(4)},
        {},
    });
    const std::vector<Document::DepthRow> rows = Document::DepthRows(clip);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].depth == 4);
    REQUIRE(rows[0].spans.size() == 1);
    CHECK(rows[0].spans[0].first_frame == 0);
    CHECK(rows[0].spans[0].last_frame == 2);
}

TEST_CASE("A depth never removed runs to the last frame") {
    const AfpAnimation::Container clip = ClipOf({
        {},
        {Place(kCreate, 7)},
        {},
        {},
    });
    const std::vector<Document::DepthRow> rows = Document::DepthRows(clip);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].depth == 7);
    REQUIRE(rows[0].spans.size() == 1);
    CHECK(rows[0].spans[0].first_frame == 1);
    CHECK(rows[0].spans[0].last_frame == 3);
}

TEST_CASE("Replacing the character at a depth starts a second span") {
    const AfpAnimation::Container clip = ClipOf({
        {Place(kCreate, 1)},
        {},
        {Place(kCreate, 1)},
        {},
    });
    const std::vector<Document::DepthRow> rows = Document::DepthRows(clip);
    REQUIRE(rows.size() == 1);
    REQUIRE(rows[0].spans.size() == 2);
    CHECK(rows[0].spans[0].first_frame == 0);
    CHECK(rows[0].spans[0].last_frame == 1);
    CHECK(rows[0].spans[1].first_frame == 2);
    CHECK(rows[0].spans[1].last_frame == 3);
}

TEST_CASE("Rows come back one per depth, in depth order") {
    const AfpAnimation::Container clip = ClipOf({
        {Place(kCreate, 9), Place(kCreate, 2), Place(kCreate, 5)},
        {Remove(5)},
        {},
    });
    const std::vector<Document::DepthRow> rows = Document::DepthRows(clip);
    REQUIRE(rows.size() == 3);
    CHECK(rows[0].depth == 2);
    CHECK(rows[1].depth == 5);
    CHECK(rows[2].depth == 9);
    CHECK(rows[1].spans[0].last_frame == 0);
    CHECK(rows[2].spans[0].last_frame == 2);
}

TEST_CASE("A clip with no frames has no rows") {
    CHECK(Document::DepthRows(AfpAnimation::Container{}).empty());
}

TEST_CASE("A frame pointing past the tag list stops the walk") {
    AfpAnimation::Container clip = ClipOf({{Place(kCreate, 3)}});
    clip.frames.push_back(AfpAnimation::Frame{.first_tag = 40, .tag_count = 2});
    const std::vector<Document::DepthRow> rows = Document::DepthRows(clip);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].spans[0].last_frame == 1);
}
