#include <catch2/catch_test_macros.hpp>

#include "document/clip.h"
#include "document/clip_edit.h"
#include "document/library_call.h"
#include "formats/afp_animation.h"
#include "formats/afp_script.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr uint16_t kSprite = 5;

AfpAnimation::Bytecode CallBytes() {
    std::vector<uint8_t> code = {AfpScript::Op::kPush,
                                 4,
                                 AfpScript::PushType::kByte,
                                 7,
                                 AfpScript::PushType::kStoredObject,
                                 AfpScript::PushType::kByte,
                                 2,
                                 AfpScript::PushType::kShortString,
                                 0,
                                 AfpScript::Op::kGetVariable,
                                 AfpScript::Op::kPush,
                                 1,
                                 AfpScript::PushType::kShortString,
                                 1,
                                 AfpScript::Op::kCallMethod,
                                 AfpScript::Op::kPop,
                                 AfpScript::Op::kEnd};
    return AfpAnimation::Bytecode{
        .flags = 0, .strings = std::vector<AfpAnimation::StringId>{1, 2}, .code = std::move(code)};
}

AfpAnimation::Placement Scripted(uint16_t depth, int32_t x) {
    AfpAnimation::Placement placement;
    placement.depth = depth;
    placement.end_frame = 2;
    placement.character = uint16_t{3};
    placement.translation = std::array<int32_t, 2>{x, 0};
    AfpAnimation::ClipActions actions;
    actions.events.push_back(
        AfpAnimation::ClipEvent{.triggers = 1, .unread_bytes = {}, .bytecode = CallBytes()});
    placement.clip_actions = std::move(actions);
    return placement;
}

AfpAnimation::Container Clip(int32_t x, int32_t focal) {
    AfpAnimation::Container clip;
    clip.frames.resize(2);
    clip.tags.push_back(AfpAnimation::Tag{Scripted(1, x)});
    AfpAnimation::Camera camera;
    camera.focal_length = focal;
    clip.tags.push_back(AfpAnimation::Tag{camera});
    clip.frames[0].tag_count = 2;
    return clip;
}

AfpAnimation::Animation Scene() {
    AfpAnimation::Animation animation;
    animation.strings = {"", "aeplib", "aep_set_set_frame"};
    animation.root = Clip(10, 100);
    AfpAnimation::Container sprite = Clip(20, 200);
    animation.root.tags.push_back(
        AfpAnimation::Tag{AfpAnimation::Sprite{.id = kSprite, .container = std::move(sprite)}});
    animation.root.frames[0].tag_count = 3;
    return animation;
}

const AfpAnimation::Placement& PlacementIn(const AfpAnimation::Animation& animation,
                                           Document::ClipId clip) {
    const AfpAnimation::Container* found = Document::FindClip(animation, clip);
    REQUIRE(found != nullptr);
    return std::get<AfpAnimation::Placement>(found->tags[0].body);
}

const AfpAnimation::Camera& CameraIn(const AfpAnimation::Animation& animation,
                                     Document::ClipId clip) {
    const AfpAnimation::Container* found = Document::FindClip(animation, clip);
    REQUIRE(found != nullptr);
    return std::get<AfpAnimation::Camera>(found->tags[1].body);
}

std::string ArgumentIn(const AfpAnimation::Animation& animation, Document::ClipId clip) {
    const AfpAnimation::Placement& placement = PlacementIn(animation, clip);
    REQUIRE(placement.clip_actions.has_value());
    if (!placement.clip_actions) return {};
    const auto call =
        Document::ReadLibraryCall(animation, placement.clip_actions->events.front().bytecode);
    REQUIRE(call.has_value());
    if (!call) return {};
    return call->arguments.back().text;
}

const Document::ClipId kRoot{};
const Document::ClipId kInSprite{.sprite = kSprite};

}

TEST_CASE("A placement field of a sprite changes the sprite and leaves the root alone") {
    AfpAnimation::Animation animation = Scene();
    const auto set =
        Document::EditPlacementField(animation, kInSprite, 1, 0, "Translation", "55, 6");
    REQUIRE(set.has_value());
    CHECK(PlacementIn(animation, kInSprite).translation == std::array<int32_t, 2>{55, 6});
    CHECK(PlacementIn(animation, kRoot).translation == std::array<int32_t, 2>{10, 0});
}

TEST_CASE("A placement field of the root changes the root and leaves the sprite alone") {
    AfpAnimation::Animation animation = Scene();
    REQUIRE(
        Document::EditPlacementField(animation, kRoot, 1, 0, "Translation", "1, 2").has_value());
    CHECK(PlacementIn(animation, kRoot).translation == std::array<int32_t, 2>{1, 2});
    CHECK(PlacementIn(animation, kInSprite).translation == std::array<int32_t, 2>{20, 0});
}

TEST_CASE("A depth that holds nothing in the clip is refused") {
    AfpAnimation::Animation animation = Scene();
    const auto set =
        Document::EditPlacementField(animation, kInSprite, 9, 0, "Translation", "1, 2");
    REQUIRE_FALSE(set.has_value());
    CHECK(set.error().find("nothing") != std::string::npos);
}

TEST_CASE("A call argument of a sprite changes the sprite's script only") {
    AfpAnimation::Animation animation = Scene();
    REQUIRE(ArgumentIn(animation, kInSprite) == "7");
    const auto set = Document::EditCallArgument(animation, kInSprite, 1, 0, 1, "42");
    if (!set) FAIL(set.error());
    CHECK(ArgumentIn(animation, kInSprite) == "42");
    CHECK(ArgumentIn(animation, kRoot) == "7");
}

TEST_CASE("A call argument past the end of the call is refused") {
    AfpAnimation::Animation animation = Scene();
    CHECK_FALSE(Document::EditCallArgument(animation, kInSprite, 1, 0, 9, "42").has_value());
}

TEST_CASE("A camera field of a sprite changes the sprite's camera only") {
    AfpAnimation::Animation animation = Scene();
    const auto set = Document::EditCameraField(animation, kInSprite, 0, "Focal length", "321");
    if (!set) FAIL(set.error());
    CHECK(CameraIn(animation, kInSprite).focal_length == 321);
    CHECK(CameraIn(animation, kRoot).focal_length == 100);
}

TEST_CASE("A frame with no camera in the clip is refused") {
    AfpAnimation::Animation animation = Scene();
    CHECK_FALSE(
        Document::EditCameraField(animation, kInSprite, 1, "Focal length", "321").has_value());
}

TEST_CASE("An edit to a sprite that is gone is refused rather than landing in the root") {
    AfpAnimation::Animation animation = Scene();
    const Document::ClipId gone{.sprite = uint16_t{77}};
    const AfpAnimation::Animation before = animation;

    const auto placement =
        Document::EditPlacementField(animation, gone, 1, 0, "Translation", "1, 2");
    REQUIRE_FALSE(placement.has_value());
    CHECK(placement.error().find("77") != std::string::npos);
    CHECK_FALSE(Document::EditCallArgument(animation, gone, 1, 0, 1, "1").has_value());
    CHECK_FALSE(Document::EditCameraField(animation, gone, 0, "Focal length", "1").has_value());
    CHECK(animation == before);
}

TEST_CASE("A clip is required by id, with a message naming it when it is gone") {
    AfpAnimation::Animation animation = Scene();
    const auto found = Document::RequireClip(animation, kInSprite);
    REQUIRE(found.has_value());
    CHECK((*found)->frames.size() == 2);

    const auto gone = Document::RequireClip(animation, Document::ClipId{.sprite = uint16_t{77}});
    REQUIRE_FALSE(gone.has_value());
    CHECK(gone.error() == "sprite 77 is no longer in this animation");
}
