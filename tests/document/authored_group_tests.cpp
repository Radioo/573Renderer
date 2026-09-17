#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/clip.h"
#include "document/keyframe_edit.h"
#include "document/keyframes.h"
#include "document/placement_effect.h"
#include "document/tags.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kUseColour = 0x8;
constexpr uint16_t kDepth = 3;
constexpr uint32_t kLast = 4;
const Document::ClipId kRoot{};

AfpAnimation::Animation Clip() {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    for (uint32_t i = 0; i <= kLast + 1; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    return animation;
}

AfpAnimation::Placement Placed(uint32_t flags) {
    AfpAnimation::Placement placement;
    placement.flags = flags;
    placement.depth = kDepth;
    placement.end_frame = kLast + 1;
    return placement;
}

AfpAnimation::Animation Scaled() {
    AfpAnimation::Animation animation = Clip();
    AfpAnimation::Placement create = Placed(kUseMatrix);
    create.character = uint16_t{7};
    create.scale = std::array<int32_t, 2>{2048, 2048};
    create.translation = std::array<int32_t, 2>{0, 0};
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{create});
    AfpAnimation::Placement moved = Placed(kUpdateExisting | kUseMatrix);
    moved.scale = std::array<int32_t, 2>{2048, 2048};
    moved.translation = std::array<int32_t, 2>{400, 0};
    Document::InsertTag(animation.root, kLast, AfpAnimation::Tag{moved});
    return animation;
}

std::vector<std::pair<uint32_t, Document::AppliedState>>
Exported(AfpAnimation::Animation& animation, const Document::AuthoredDepth& authored) {
    const auto baked = Document::BakedFor(animation, authored);
    REQUIRE(baked.has_value());
    const auto written = Document::WriteAuthored(animation, authored, *baked);
    const std::string error = written.has_value() ? std::string() : written.error();
    INFO(error);
    REQUIRE(written.has_value());
    return Document::ReplayDepth(animation.root, kDepth, 0, kLast);
}

}

TEST_CASE("Easing a translation keeps a held scale on every frame the game draws") {
    AfpAnimation::Animation animation = Scaled();
    auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    REQUIRE(Document::SetKeyEaseAt(owned->authored, "Translation", 0, Document::Ease::Linear, {})
                .has_value());

    const auto shown = Exported(animation, owned->authored);
    REQUIRE(shown.size() == kLast + 1);
    for (const auto& [frame, state] : shown) {
        INFO("frame " << frame);
        CHECK(state.matrix[0] == 2.0);
        CHECK(state.matrix[3] == 2.0);
        CHECK(state.matrix[4] == 100.0 * frame);
    }
}

TEST_CASE("Easing a colour keeps a held add colour on every frame the game draws") {
    AfpAnimation::Animation animation = Clip();
    AfpAnimation::Placement create = Placed(kUseMatrix | kUseColour);
    create.character = uint16_t{7};
    create.multiply_colour = std::array<int16_t, 4>{0, 0, 0, 0};
    create.add_colour = std::array<int16_t, 4>{255, 0, 0, 0};
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{create});
    AfpAnimation::Placement bright = Placed(kUpdateExisting | kUseColour);
    bright.multiply_colour = std::array<int16_t, 4>{255, 255, 255, 255};
    bright.add_colour = std::array<int16_t, 4>{255, 0, 0, 0};
    Document::InsertTag(animation.root, kLast, AfpAnimation::Tag{bright});

    auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    REQUIRE(
        Document::SetKeyEaseAt(owned->authored, "Multiply colour", 0, Document::Ease::Linear, {})
            .has_value());

    for (const auto& [frame, state] : Exported(animation, owned->authored)) {
        INFO("frame " << frame);
        CHECK(state.add[0] == 1.0);
    }
}

TEST_CASE("A frame that resets a scale the depth animates is owned as a key at the identity") {
    AfpAnimation::Animation animation = Clip();
    AfpAnimation::Placement create = Placed(kUseMatrix);
    create.character = uint16_t{7};
    create.scale = std::array<int32_t, 2>{2048, 2048};
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{create});
    AfpAnimation::Placement moved = Placed(kUpdateExisting | kUseMatrix);
    moved.translation = std::array<int32_t, 2>{50, 0};
    Document::InsertTag(animation.root, 2, AfpAnimation::Tag{moved});

    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    CHECK(Document::SampleTrack(*std::ranges::find(owned->authored.tracks, std::string("Scale"),
                                                   &Document::Track::property),
                                3) == std::vector<int64_t>{1024, 1024});
    CHECK(
        Document::SampleTrack(*std::ranges::find(owned->authored.tracks, std::string("Translation"),
                                                 &Document::Track::property),
                              1) == std::vector<int64_t>{0, 0});

    const AfpAnimation::Animation before = animation;
    const auto baked = Document::BakedFor(animation, owned->authored);
    REQUIRE(baked.has_value());
    REQUIRE(Document::WriteAuthored(animation, owned->authored, *baked).has_value());
    CHECK(animation == before);
}

TEST_CASE("What the keyframes say is what the game draws on every exported frame") {
    AfpAnimation::Animation animation = Scaled();
    auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    REQUIRE(Document::SetKeyEaseAt(owned->authored, "Translation", 0, Document::Ease::Linear, {})
                .has_value());
    REQUIRE(Document::SetKeyValueAt(owned->authored, "Scale", kLast, "1024, 4096").has_value());
    REQUIRE(Document::SetKeyEaseAt(owned->authored, "Scale", 0, Document::Ease::Bezier,
                                   Document::Bezier{.x1 = 0.3, .y1 = 0.0, .x2 = 0.7, .y2 = 1.0})
                .has_value());

    const auto shown = Exported(animation, owned->authored);
    const auto baked = Document::BakedFor(animation, owned->authored);
    REQUIRE(baked.has_value());
    for (const auto& [frame, state] : shown) {
        INFO("frame " << frame);
        CHECK(state == Document::KeyedState(owned->authored, *baked, frame));
    }
}

TEST_CASE("A span the game would draw differently from its keyframes is caught") {
    AfpAnimation::Animation animation = Scaled();
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    if (!owned) return;
    CHECK(Document::CheckDrawnAsKeyed(animation.root, owned->authored, owned->baked).has_value());

    const auto& moved = animation.root.frames[kLast];
    auto& placement = std::get<AfpAnimation::Placement>(animation.root.tags[moved.first_tag].body);
    placement.scale.reset();
    const auto caught = Document::CheckDrawnAsKeyed(animation.root, owned->authored, owned->baked);
    REQUIRE_FALSE(caught.has_value());
    CHECK(caught.error().find("frame 4") != std::string::npos);
}
