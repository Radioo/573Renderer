#include <catch2/catch_test_macros.hpp>

#include "document/frame_notes.h"
#include "formats/afp_animation.h"

#include <cstdint>
#include <vector>

namespace {

AfpAnimation::Tag Place(uint16_t depth) {
    AfpAnimation::Placement placement;
    placement.flags = 0x2;
    placement.depth = depth;
    return AfpAnimation::Tag{placement};
}

AfpAnimation::Tag Script() {
    return AfpAnimation::Tag{AfpAnimation::Action{}};
}

AfpAnimation::Tag Camera() {
    return AfpAnimation::Tag{AfpAnimation::Camera{}};
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

TEST_CASE("Frame notes name the frames carrying a script or a camera") {
    const AfpAnimation::Container clip =
        ClipOf({{Place(1)}, {Script()}, {Place(2), Camera()}, {}, {Camera(), Script()}});
    const std::vector<Document::FrameNote> notes = Document::FrameNotes(clip);
    REQUIRE(notes.size() == 3);
    CHECK(notes[0].frame == 1);
    CHECK(notes[0].script);
    CHECK_FALSE(notes[0].camera);
    CHECK(notes[1].frame == 2);
    CHECK_FALSE(notes[1].script);
    CHECK(notes[1].camera);
    CHECK(notes[2].frame == 4);
    CHECK(notes[2].script);
    CHECK(notes[2].camera);
}

TEST_CASE("A clip with nothing to say has no frame notes") {
    CHECK(Document::FrameNotes(ClipOf({{Place(1)}, {}, {Place(2)}})).empty());
    CHECK(Document::FrameNotes(AfpAnimation::Container{}).empty());
}

TEST_CASE("A frame reaching past the tags it has is read as far as it goes") {
    AfpAnimation::Container clip = ClipOf({{Script()}});
    clip.frames.front().tag_count = 6;
    const std::vector<Document::FrameNote> notes = Document::FrameNotes(clip);
    REQUIRE(notes.size() == 1);
    CHECK(notes.front().script);
}
