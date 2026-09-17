#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/clip.h"
#include "document/keyframe_edit.h"
#include "document/key_selection.h"
#include "document/keyframes.h"
#include "document/placement_effect.h"
#include "document/tags.h"
#include "formats/afp_animation.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kThreeD = 0x04000000;
constexpr uint16_t kDepth = 3;
constexpr uint32_t kLast = 4;
const Document::ClipId kRoot{};

AfpAnimation::Animation Moving(uint32_t mode) {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    for (uint32_t i = 0; i <= kLast + 1; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    AfpAnimation::Placement create;
    create.flags = kUseMatrix | mode;
    create.depth = kDepth;
    create.end_frame = kLast + 1;
    create.character = uint16_t{7};
    create.translation = std::array<int32_t, 2>{0, 0};
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{create});
    AfpAnimation::Placement moved;
    moved.flags = kUpdateExisting | kUseMatrix | mode;
    moved.depth = kDepth;
    moved.end_frame = kLast + 1;
    moved.translation = std::array<int32_t, 2>{80, 0};
    Document::InsertTag(animation.root, kLast, AfpAnimation::Tag{moved});
    return animation;
}

Document::OwnedDepth Own(const AfpAnimation::Animation& animation) {
    auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    return std::move(*owned);
}

}

TEST_CASE("The properties a depth could start animating are the groups it does not yet") {
    const Document::OwnedDepth owned = Own(Moving(0));
    CHECK(Document::PropertiesToAdd(owned.authored, owned.baked) ==
          std::vector<std::string>{"Scale", "Rotate skew", "Multiply colour", "Add colour"});
}

TEST_CASE("A 3D depth can only start animating its colours") {
    const Document::OwnedDepth owned = Own(Moving(kThreeD));
    CHECK(Document::PropertiesToAdd(owned.authored, owned.baked) ==
          std::vector<std::string>{"Multiply colour", "Add colour"});
}

TEST_CASE("A property started on a depth draws nothing new until it is changed") {
    AfpAnimation::Animation animation = Moving(0);
    Document::OwnedDepth owned = Own(animation);
    const AfpAnimation::Animation before = animation;
    REQUIRE(Document::AddTrack(owned.authored, owned.baked, "Scale").has_value());

    const auto key = Document::KeyAt(owned.authored, "Scale", 0);
    REQUIRE(key.has_value());
    if (!key) return;
    CHECK(key->value == std::vector<int64_t>{1024, 1024});
    const auto baked = Document::BakedFor(animation, owned.authored);
    REQUIRE(baked.has_value());
    if (!baked) return;
    REQUIRE(Document::WriteAuthored(animation, owned.authored, *baked).has_value());
    CHECK(animation == before);
}

TEST_CASE("A property started on a depth animates once its keyframes change") {
    AfpAnimation::Animation animation = Moving(0);
    Document::OwnedDepth owned = Own(animation);
    REQUIRE(Document::AddTrack(owned.authored, owned.baked, "Scale").has_value());
    REQUIRE(Document::AddKeyAt(owned.authored, "Scale", kLast).has_value());
    REQUIRE(Document::SetKeyValueAt(owned.authored, "Scale", kLast, "2048, 2048").has_value());
    REQUIRE(Document::SetKeysEase(owned.authored,
                                  {Document::KeyRef{.property = "Scale", .frame = 0}},
                                  Document::Ease::Linear, {})
                .has_value());

    const auto baked = Document::BakedFor(animation, owned.authored);
    REQUIRE(baked.has_value());
    if (!baked) return;
    REQUIRE(Document::WriteAuthored(animation, owned.authored, *baked).has_value());
    const auto shown = Document::ReplayDepth(animation.root, kDepth, 0, kLast);
    REQUIRE(shown.size() == kLast + 1);
    for (const auto& [frame, state] : shown) {
        INFO("frame " << frame);
        CHECK(state == Document::KeyedState(owned.authored, *baked, frame));
    }
    CHECK(shown.back().second.matrix[0] == 2.0);
    CHECK(shown.front().second.matrix[0] == 1.0);
}

TEST_CASE("A property that is already animated or cannot be started is refused") {
    Document::OwnedDepth owned = Own(Moving(0));
    const auto again = Document::AddTrack(owned.authored, owned.baked, "Translation");
    REQUIRE_FALSE(again.has_value());
    CHECK(again.error().find("Translation") != std::string::npos);
    CHECK_FALSE(Document::AddTrack(owned.authored, owned.baked, "Ratio").has_value());
    CHECK_FALSE(Document::AddTrack(owned.authored, owned.baked, "Short scale").has_value());
    CHECK_FALSE(Document::AddTrack(owned.authored, owned.baked, "nonsense").has_value());
    CHECK(owned.authored.tracks.size() == 1);
}
