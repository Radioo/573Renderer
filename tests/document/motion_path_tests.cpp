#include <catch2/catch_test_macros.hpp>

#include "document/keyframes.h"
#include "document/motion_path.h"
#include "document/tags.h"
#include "formats/afp_animation.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kThreeD = 0x04000000;
constexpr uint16_t kDepth = 2;

AfpAnimation::Container Frames(std::size_t count) {
    AfpAnimation::Container clip;
    for (std::size_t i = 0; i < count; i++)
        clip.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    return clip;
}

void Place(AfpAnimation::Container& clip, uint32_t frame, uint32_t flags, int32_t x, int32_t y) {
    AfpAnimation::Placement placement;
    placement.flags = flags | kUseMatrix;
    placement.depth = kDepth;
    if ((flags & kUpdateExisting) == 0) placement.character = uint16_t{1};
    placement.translation = std::array<int32_t, 2>{x, y};
    Document::InsertTag(clip, frame, AfpAnimation::Tag{placement});
}

void Remove(AfpAnimation::Container& clip, uint32_t frame) {
    Document::InsertTag(clip, frame,
                        AfpAnimation::Tag{AfpAnimation::Remove{.unread_word = 0, .depth = kDepth}});
}

std::vector<uint32_t> FramesOf(const std::vector<Document::PathPoint>& path) {
    std::vector<uint32_t> frames;
    frames.reserve(path.size());
    for (const Document::PathPoint& point : path)
        frames.push_back(point.frame);
    return frames;
}

Document::Keyframe Key(uint32_t frame, std::vector<int64_t> value) {
    Document::Keyframe key;
    key.frame = frame;
    key.value = std::move(value);
    return key;
}

AfpAnimation::Container Moving() {
    AfpAnimation::Container clip = Frames(7);
    Place(clip, 0, 0, 200, 400);
    Place(clip, 2, kUpdateExisting, 400, 400);
    Place(clip, 4, kUpdateExisting, 400, 800);
    Remove(clip, 5);
    return clip;
}

}

TEST_CASE("The motion path follows the depth's translation in stage pixels over its span") {
    const std::vector<Document::PathPoint> path = Document::MotionPath(Moving(), kDepth, 1, {});
    CHECK(path == std::vector<Document::PathPoint>{
                      {.frame = 0, .at = {10.0, 20.0}, .keyed = false},
                      {.frame = 1, .at = {10.0, 20.0}, .keyed = false},
                      {.frame = 2, .at = {20.0, 20.0}, .keyed = false},
                      {.frame = 3, .at = {20.0, 20.0}, .keyed = false},
                      {.frame = 4, .at = {20.0, 40.0}, .keyed = false},
                  });
}

TEST_CASE("The keyed points are the keyframes of the Translation track") {
    const std::vector<Document::Track> tracks{
        {.property = "Rotation", .keys = {Key(2, {0})}},
        {.property = "Translation", .keys = {Key(0, {200, 400}), Key(4, {400, 800})}},
    };
    std::vector<uint32_t> keyed;
    for (const Document::PathPoint& point : Document::MotionPath(Moving(), kDepth, 3, tracks)) {
        if (point.keyed) keyed.push_back(point.frame);
    }
    CHECK(keyed == std::vector<uint32_t>{0, 4});
}

TEST_CASE("The motion path covers only the span under the frame") {
    AfpAnimation::Container clip = Moving();
    Place(clip, 6, 0, 0, 0);
    CHECK(FramesOf(Document::MotionPath(clip, kDepth, 6, {})) == std::vector<uint32_t>{6});
    CHECK(FramesOf(Document::MotionPath(clip, kDepth, 4, {})) ==
          std::vector<uint32_t>{0, 1, 2, 3, 4});
    CHECK(Document::MotionPath(clip, kDepth, 5, {}).empty());
    CHECK(Document::MotionPath(clip, kDepth + 1, 1, {}).empty());
}

TEST_CASE("A 3D span has no motion path on the flat stage") {
    AfpAnimation::Container clip = Frames(4);
    Place(clip, 0, 0, 200, 400);
    Place(clip, 2, kUpdateExisting | kThreeD, 400, 400);
    CHECK(Document::MotionPath(clip, kDepth, 0, {}).empty());
}
