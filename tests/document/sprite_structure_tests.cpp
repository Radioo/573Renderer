#include <catch2/catch_test_macros.hpp>

#include "document/animation_strings.h"
#include "document/camera_edit.h"
#include "document/clip.h"
#include "document/frame_edit.h"
#include "document/label_edit.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr uint16_t kSprite = 5;
const Document::ClipId kInSprite{.sprite = kSprite};
const Document::ClipId kRoot{};

AfpAnimation::Container Frames(std::size_t count, uint32_t first_tag) {
    AfpAnimation::Container clip;
    for (std::size_t i = 0; i < count; i++)
        clip.frames.push_back(AfpAnimation::Frame{.first_tag = first_tag, .tag_count = 0});
    return clip;
}

AfpAnimation::Animation Scene() {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    animation.root = Frames(3, 1);
    animation.root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Sprite{.id = kSprite, .container = Frames(6, 0)}});
    return animation;
}

const AfpAnimation::Container& ClipOf(const AfpAnimation::Animation& animation,
                                      Document::ClipId clip) {
    const AfpAnimation::Container* found = Document::FindClip(animation, clip);
    REQUIRE(found != nullptr);
    return *found;
}

std::vector<std::string> LabelsOf(const AfpAnimation::Animation& animation, Document::ClipId clip) {
    std::vector<std::string> names;
    for (const AfpAnimation::Label& label : ClipOf(animation, clip).labels)
        names.push_back(Document::StringText(animation, label.name));
    return names;
}

}

TEST_CASE("A depth added inside a sprite lands in the sprite") {
    AfpAnimation::Animation animation = Scene();
    REQUIRE(Document::AddDepth(animation, kInSprite, 2, 9, 1, 4).has_value());

    const std::vector<Document::DepthRow> rows = Document::DepthRows(ClipOf(animation, kInSprite));
    REQUIRE(rows.size() == 1);
    CHECK(rows.front().depth == 2);
    REQUIRE(rows.front().spans.size() == 1);
    CHECK(rows.front().spans.front().first_frame == 1);
    CHECK(rows.front().spans.front().last_frame == 4);
    CHECK(Document::DepthRows(ClipOf(animation, kRoot)).empty());
}

TEST_CASE("A sprite's own frame count bounds a depth added inside it") {
    AfpAnimation::Animation animation = Scene();
    CHECK(Document::AddDepth(animation, kInSprite, 2, 9, 0, 5).has_value());
    CHECK_FALSE(Document::AddDepth(animation, kRoot, 3, 9, 0, 5).has_value());
}

TEST_CASE("A depth removed inside a sprite leaves the root alone") {
    AfpAnimation::Animation animation = Scene();
    REQUIRE(Document::AddDepth(animation, kInSprite, 2, 9, 0, 2).has_value());
    REQUIRE(Document::AddDepth(animation, kRoot, 2, 9, 0, 1).has_value());
    REQUIRE(Document::RemoveDepth(animation, kInSprite, 2, 1).has_value());

    CHECK(Document::DepthRows(ClipOf(animation, kInSprite)).empty());
    CHECK(Document::DepthRows(ClipOf(animation, kRoot)).size() == 1);
}

TEST_CASE("Frames inserted and removed inside a sprite change only the sprite") {
    AfpAnimation::Animation animation = Scene();
    REQUIRE(Document::InsertFrame(animation, kInSprite, 2).has_value());
    CHECK(ClipOf(animation, kInSprite).frames.size() == 7);
    CHECK(ClipOf(animation, kRoot).frames.size() == 3);

    REQUIRE(Document::RemoveFrame(animation, kInSprite, 0).has_value());
    REQUIRE(Document::RemoveFrame(animation, kInSprite, 0).has_value());
    CHECK(ClipOf(animation, kInSprite).frames.size() == 5);
    CHECK(ClipOf(animation, kRoot).frames.size() == 3);
}

TEST_CASE("Removing the root frame that defines a sprite keeps the sprite editable") {
    AfpAnimation::Animation animation = Scene();
    animation.root.frames[0].first_tag = 0;
    animation.root.frames[0].tag_count = 1;
    REQUIRE(Document::RemoveFrame(animation, kRoot, 0).has_value());
    REQUIRE(Document::FindClip(animation, kInSprite) != nullptr);
    CHECK(Document::AddLabel(animation, kInSprite, "still here", 0).has_value());
}

TEST_CASE("Labels of a sprite are its own") {
    AfpAnimation::Animation animation = Scene();
    REQUIRE(Document::AddLabel(animation, kInSprite, "spin", 3).has_value());
    REQUIRE(Document::AddLabel(animation, kRoot, "spin", 1).has_value());
    CHECK(LabelsOf(animation, kInSprite) == std::vector<std::string>{"spin"});
    CHECK(LabelsOf(animation, kRoot) == std::vector<std::string>{"spin"});

    REQUIRE(Document::RenameLabel(animation, kInSprite, "spin", "turn").has_value());
    CHECK(LabelsOf(animation, kInSprite) == std::vector<std::string>{"turn"});
    CHECK(LabelsOf(animation, kRoot) == std::vector<std::string>{"spin"});

    REQUIRE(Document::MoveLabel(animation, kInSprite, "turn", 5).has_value());
    CHECK(ClipOf(animation, kInSprite).labels.front().frame == 5);
    CHECK(ClipOf(animation, kRoot).labels.front().frame == 1);

    REQUIRE(Document::RemoveLabel(animation, kInSprite, "turn").has_value());
    CHECK(LabelsOf(animation, kInSprite).empty());
    CHECK(LabelsOf(animation, kRoot) == std::vector<std::string>{"spin"});
}

TEST_CASE("A sprite label is bounded by the sprite's frames, not the root's") {
    AfpAnimation::Animation animation = Scene();
    CHECK(Document::AddLabel(animation, kInSprite, "late", 5).has_value());
    CHECK_FALSE(Document::AddLabel(animation, kRoot, "late", 5).has_value());
}

TEST_CASE("Sprite labels are kept in name order, as shipped sprites keep them") {
    AfpAnimation::Animation animation = Scene();
    REQUIRE(Document::AddLabel(animation, kInSprite, "b", 0).has_value());
    REQUIRE(Document::AddLabel(animation, kInSprite, "c", 1).has_value());
    REQUIRE(Document::AddLabel(animation, kInSprite, "a", 2).has_value());
    CHECK(LabelsOf(animation, kInSprite) == std::vector<std::string>{"a", "b", "c"});
}

TEST_CASE("A camera added inside a sprite belongs to the sprite") {
    AfpAnimation::Animation animation = Scene();
    REQUIRE(Document::AddCamera(animation, kInSprite, 4, 1).has_value());
    CHECK(Document::CameraTag(ClipOf(animation, kInSprite), 4).has_value());
    CHECK_FALSE(Document::CameraTag(ClipOf(animation, kRoot), 1).has_value());

    REQUIRE(Document::RemoveCamera(animation, kInSprite, 4).has_value());
    CHECK_FALSE(Document::CameraTag(ClipOf(animation, kInSprite), 4).has_value());
}

TEST_CASE("Every structure edit refuses a sprite that is gone and changes nothing") {
    AfpAnimation::Animation animation = Scene();
    const Document::ClipId gone{.sprite = uint16_t{77}};
    const AfpAnimation::Animation before = animation;

    CHECK_FALSE(Document::AddDepth(animation, gone, 2, 9, 0, 1).has_value());
    CHECK_FALSE(Document::RemoveDepth(animation, gone, 2, 0).has_value());
    CHECK_FALSE(Document::InsertFrame(animation, gone, 0).has_value());
    CHECK_FALSE(Document::RemoveFrame(animation, gone, 0).has_value());
    CHECK_FALSE(Document::AddLabel(animation, gone, "x", 0).has_value());
    CHECK_FALSE(Document::RenameLabel(animation, gone, "x", "y").has_value());
    CHECK_FALSE(Document::MoveLabel(animation, gone, "x", 0).has_value());
    CHECK_FALSE(Document::RemoveLabel(animation, gone, "x").has_value());
    CHECK_FALSE(Document::AddCamera(animation, gone, 0, 1).has_value());
    CHECK_FALSE(Document::RemoveCamera(animation, gone, 0).has_value());
    CHECK(animation == before);

    const auto refused = Document::AddLabel(animation, gone, "x", 0);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error() == Document::MissingClipMessage(gone));
}
