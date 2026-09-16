#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/keyframes.h"
#include "document/tags.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

Document::Keyframe Key(uint32_t frame, std::vector<int64_t> value) {
    return Document::Keyframe{
        .frame = frame, .value = std::move(value), .ease = Document::Ease::Linear, .bezier = {}};
}

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint16_t kDepth = 3;

AfpAnimation::Animation Clip(std::size_t frames) {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    for (std::size_t i = 0; i < frames; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    return animation;
}

AfpAnimation::Placement Create(uint16_t end_frame, int32_t x) {
    AfpAnimation::Placement placement;
    placement.depth = kDepth;
    placement.end_frame = end_frame;
    placement.character = uint16_t{7};
    placement.translation = std::array<int32_t, 2>{x, 0};
    return placement;
}

AfpAnimation::Placement Update(uint16_t end_frame) {
    AfpAnimation::Placement placement;
    placement.flags = kUpdateExisting;
    placement.depth = kDepth;
    placement.end_frame = end_frame;
    return placement;
}

const Document::Track* TrackFor(const Document::AuthoredDepth& authored, const std::string& name) {
    const auto found = std::ranges::find(authored.tracks, name, &Document::Track::property);
    return found == authored.tracks.end() ? nullptr : &*found;
}

std::vector<uint32_t> FramesPlacing(const AfpAnimation::Container& clip, uint16_t depth) {
    std::vector<uint32_t> frames;
    for (uint32_t frame = 0; frame < clip.frames.size(); frame++) {
        const AfpAnimation::Frame& owner = clip.frames[frame];
        for (uint32_t i = 0; i < owner.tag_count; i++) {
            const auto* placement =
                std::get_if<AfpAnimation::Placement>(&clip.tags[owner.first_tag + i].body);
            if (placement != nullptr && placement->depth == depth) frames.push_back(frame);
        }
    }
    return frames;
}

AfpAnimation::Animation Moving() {
    AfpAnimation::Animation animation = Clip(5);
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{Create(4, 100)});
    for (uint32_t frame = 1; frame < 4; frame++) {
        AfpAnimation::Placement update = Update(4);
        update.translation = std::array<int32_t, 2>{100 + (static_cast<int32_t>(frame) * 10), 0};
        Document::InsertTag(animation.root, frame, AfpAnimation::Tag{update});
    }
    return animation;
}

}

TEST_CASE("Owning a depth keys every frame the property is set on") {
    const AfpAnimation::Animation animation = Moving();
    const auto owned = Document::OwnDepth(animation, "afp/a", kDepth, 2);
    if (!owned) FAIL(owned.error());

    CHECK(owned->authored.animation == "afp/a");
    CHECK(owned->authored.depth == kDepth);
    CHECK(owned->authored.first_frame == 0);
    CHECK(owned->authored.last_frame == 4);
    REQUIRE(owned->authored.tracks.size() == 1);

    const Document::Track* track = TrackFor(owned->authored, "Translation");
    REQUIRE(track != nullptr);
    REQUIRE(track->keys.size() == 4);
    for (std::size_t i = 0; i < track->keys.size(); i++) {
        CHECK(track->keys[i].frame == i);
        CHECK(track->keys[i].value ==
              std::vector<int64_t>{100 + (static_cast<int64_t>(i) * 10), 0});
        CHECK(track->keys[i].ease == Document::Ease::Hold);
    }
}

TEST_CASE("The create placement keeps what a keyframe cannot hold and drops what it can") {
    const AfpAnimation::Animation animation = Moving();
    const auto owned = Document::OwnDepth(animation, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    REQUIRE(owned->baked.create.character.has_value());
    CHECK(*owned->baked.create.character == 7);
    CHECK(owned->baked.create.end_frame == 4);
    CHECK_FALSE(owned->baked.create.translation.has_value());
    CHECK((owned->baked.create.flags & kUpdateExisting) == 0);
}

TEST_CASE("Owning a depth and detaching it again leaves the clip as it was") {
    const AfpAnimation::Animation before = Moving();
    AfpAnimation::Animation animation = before;
    const auto owned = Document::OwnDepth(animation, "afp/a", kDepth, 1);
    REQUIRE(owned.has_value());

    const auto detached = Document::WriteAuthored(animation, owned->authored, owned->baked);
    if (!detached) FAIL(detached.error());
    CHECK(animation.root == before.root);
}

TEST_CASE("A depth that only ever places once still owns and detaches") {
    AfpAnimation::Animation animation = Clip(3);
    Document::InsertTag(animation.root, 1, AfpAnimation::Tag{Create(2, 50)});
    const AfpAnimation::Animation before = animation;

    const auto owned = Document::OwnDepth(animation, "afp/a", kDepth, 1);
    REQUIRE(owned.has_value());
    CHECK(owned->authored.first_frame == 1);
    CHECK(owned->authored.last_frame == 2);
    REQUIRE(Document::WriteAuthored(animation, owned->authored, owned->baked).has_value());
    CHECK(animation.root == before.root);
}

TEST_CASE("A depth carrying something a keyframe cannot hold is not owned") {
    AfpAnimation::Animation animation = Moving();
    AfpAnimation::Placement named = Update(4);
    named.name = AfpAnimation::StringId{0};
    Document::InsertTag(animation.root, 4, AfpAnimation::Tag{named});
    const auto owned = Document::OwnDepth(animation, "afp/a", kDepth, 0);
    REQUIRE_FALSE(owned.has_value());
    CHECK(owned.error().find("instance name") != std::string::npos);
}

TEST_CASE("A depth placed a second time inside its span is not owned") {
    AfpAnimation::Animation animation = Moving();
    Document::InsertTag(animation.root, 4, AfpAnimation::Tag{Create(5, 400)});
    const auto owned = Document::OwnDepth(animation, "afp/a", kDepth, 0);
    REQUIRE_FALSE(owned.has_value());
    CHECK(owned.error().find("again") != std::string::npos);
}

TEST_CASE("A depth whose frames end somewhere else is not owned") {
    AfpAnimation::Animation animation = Clip(3);
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{Create(3, 0)});
    AfpAnimation::Placement update = Update(2);
    update.translation = std::array<int32_t, 2>{10, 0};
    Document::InsertTag(animation.root, 1, AfpAnimation::Tag{update});
    const auto owned = Document::OwnDepth(animation, "afp/a", kDepth, 0);
    REQUIRE_FALSE(owned.has_value());
    CHECK(owned.error().find("ends") != std::string::npos);
}

TEST_CASE("Nothing at the frame is not owned") {
    const AfpAnimation::Animation animation = Clip(3);
    CHECK_FALSE(Document::OwnDepth(animation, "afp/a", kDepth, 0).has_value());
    CHECK_FALSE(Document::OwnDepth(animation, "afp/a", kDepth, 9).has_value());
}

TEST_CASE("Holding a keyframe writes only the frames that are keyed") {
    AfpAnimation::Animation animation = Clip(6);
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{Create(6, 0)});
    AfpAnimation::Placement update = Update(6);
    update.translation = std::array<int32_t, 2>{500, 0};
    Document::InsertTag(animation.root, 4, AfpAnimation::Tag{update});

    auto owned = Document::OwnDepth(animation, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    const auto placements = Document::AuthoredPlacements(owned->authored, owned->baked);
    REQUIRE(placements.has_value());
    REQUIRE(placements->size() == 2);
    CHECK((*placements)[0].first == 0);
    CHECK((*placements)[1].first == 4);
}

TEST_CASE("Easing between two keyframes writes every frame between them") {
    AfpAnimation::Animation animation = Clip(6);
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{Create(6, 0)});
    AfpAnimation::Placement update = Update(6);
    update.translation = std::array<int32_t, 2>{400, 0};
    Document::InsertTag(animation.root, 4, AfpAnimation::Tag{update});

    auto owned = Document::OwnDepth(animation, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    REQUIRE(owned->authored.tracks.size() == 1);
    REQUIRE(Document::SetKeyframeEase(owned->authored.tracks.front(), 0, Document::Ease::Linear, {})
                .has_value());

    const auto placements = Document::AuthoredPlacements(owned->authored, owned->baked);
    REQUIRE(placements.has_value());
    REQUIRE(placements->size() == 5);
    for (std::size_t i = 0; i < placements->size(); i++) {
        CHECK((*placements)[i].first == i);
        const AfpAnimation::Placement& placement = (*placements)[i].second;
        REQUIRE(placement.translation.has_value());
        CHECK((*placement.translation)[0] == static_cast<int64_t>(i) * 100);
    }
    CHECK((*placements)[0].second.character.has_value());
    CHECK_FALSE((*placements)[1].second.character.has_value());
    CHECK(((*placements)[1].second.flags & kUpdateExisting) != 0);
}

TEST_CASE("Detaching an eased depth replaces the frames the baked data had") {
    AfpAnimation::Animation animation = Clip(6);
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{Create(6, 0)});
    AfpAnimation::Placement update = Update(6);
    update.translation = std::array<int32_t, 2>{400, 0};
    Document::InsertTag(animation.root, 4, AfpAnimation::Tag{update});

    auto owned = Document::OwnDepth(animation, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    REQUIRE(Document::SetKeyframeEase(owned->authored.tracks.front(), 0, Document::Ease::Linear, {})
                .has_value());
    REQUIRE(Document::WriteAuthored(animation, owned->authored, owned->baked).has_value());

    CHECK(FramesPlacing(animation.root, kDepth) == std::vector<uint32_t>{0, 1, 2, 3, 4});
    const auto again = Document::OwnDepth(animation, "afp/a", kDepth, 0);
    REQUIRE(again.has_value());
    REQUIRE(again->authored.tracks.size() == 1);
    CHECK(again->authored.tracks.front().keys.size() == 5);
}

TEST_CASE("Removing keyframes and detaching drops the frames they wrote") {
    AfpAnimation::Animation animation = Moving();
    auto owned = Document::OwnDepth(animation, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    REQUIRE(Document::RemoveKeyframe(owned->authored.tracks.front(), 2).has_value());
    REQUIRE(Document::RemoveKeyframe(owned->authored.tracks.front(), 3).has_value());
    REQUIRE(Document::WriteAuthored(animation, owned->authored, owned->baked).has_value());
    CHECK(FramesPlacing(animation.root, kDepth) == std::vector<uint32_t>{0, 1});
}

TEST_CASE("Keyframes outside the authored range are refused rather than written") {
    AfpAnimation::Animation animation = Moving();
    auto owned = Document::OwnDepth(animation, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    REQUIRE(Document::AddKeyframe(owned->authored.tracks.front(), Key(9, {900, 0})).has_value());
    CHECK_FALSE(Document::AuthoredPlacements(owned->authored, owned->baked).has_value());
    CHECK_FALSE(Document::WriteAuthored(animation, owned->authored, owned->baked).has_value());
}
