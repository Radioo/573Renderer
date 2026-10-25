#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/clip.h"
#include "document/frame_edit.h"
#include "document/keyframes.h"
#include "document/placement_effect.h"
#include "document/span_trim.h"
#include "document/tags.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint32_t kUseColour = 0x8;
constexpr uint32_t kThreeD = 0x04000000;
constexpr uint16_t kDepth = 2;
const Document::ClipId kRoot{};

AfpAnimation::Animation Clip(std::size_t frames) {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    for (std::size_t i = 0; i < frames; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    return animation;
}

void Add(AfpAnimation::Animation& animation, uint16_t depth, uint32_t first, uint32_t last) {
    REQUIRE(Document::AddDepth(animation, kRoot, depth, 7, first, last).has_value());
}

AfpAnimation::Placement Update(uint32_t flags) {
    AfpAnimation::Placement update;
    update.flags = kUpdateExisting | flags;
    update.depth = kDepth;
    return update;
}

AfpAnimation::Curve Curve(uint8_t slot, std::size_t points, int32_t value) {
    return AfpAnimation::Curve{
        .slot = slot, .flags = 0, .values = std::vector<int32_t>(points * 2, value)};
}

AfpAnimation::Placement Curved(std::vector<AfpAnimation::Curve> curves) {
    AfpAnimation::Placement update = Update(0);
    update.curves = std::move(curves);
    return update;
}

std::vector<Document::Span> SpansOf(const AfpAnimation::Animation& animation, uint16_t depth) {
    for (const Document::DepthRow& row : Document::DepthRows(animation.root)) {
        if (row.depth == depth) return row.spans;
    }
    return {};
}

std::vector<uint32_t> PlacementFrames(const AfpAnimation::Animation& animation) {
    std::vector<uint32_t> frames;
    for (uint32_t frame = 0; frame < animation.root.frames.size(); frame++) {
        const AfpAnimation::Frame& owner = animation.root.frames[frame];
        for (uint32_t i = 0; i < owner.tag_count; i++) {
            const auto* placement = std::get_if<AfpAnimation::Placement>(
                &animation.root.tags[owner.first_tag + i].body);
            if (placement != nullptr && placement->depth == kDepth) frames.push_back(frame);
        }
    }
    return frames;
}

std::vector<uint16_t> EndFrames(const AfpAnimation::Animation& animation) {
    std::vector<uint16_t> ends;
    for (const AfpAnimation::Tag& tag : animation.root.tags) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        if (placement != nullptr && placement->depth == kDepth && placement->end_frame != 0)
            ends.push_back(placement->end_frame);
    }
    return ends;
}

const AfpAnimation::Placement& CreateOf(const AfpAnimation::Animation& animation) {
    const auto found = std::ranges::find_if(animation.root.tags, [](const AfpAnimation::Tag& tag) {
        const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body);
        return placement != nullptr && placement->depth == kDepth &&
               (placement->flags & kUpdateExisting) == 0;
    });
    REQUIRE(found != animation.root.tags.end());
    return std::get<AfpAnimation::Placement>(found->body);
}

std::string Error(const auto& result) {
    return result.has_value() ? std::string() : result.error();
}

Document::Keyframe Key(uint32_t frame, int64_t value, Document::Ease ease) {
    return Document::Keyframe{.frame = frame, .value = {value}, .ease = ease, .bezier = {}};
}

}

TEST_CASE("Trimming a span's end drops the later updates and closes it sooner") {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, kDepth, 1, 6);
    Document::InsertTag(animation.root, 3, AfpAnimation::Tag{Update(kUseMatrix)});
    Document::InsertTag(animation.root, 5, AfpAnimation::Tag{Update(kUseMatrix)});
    const auto trimmed =
        Document::TrimSpan(animation, kRoot, kDepth, 2, {.first_frame = 1, .last_frame = 3});
    INFO(Error(trimmed));
    REQUIRE(trimmed.has_value());
    CHECK(SpansOf(animation, kDepth) == std::vector<Document::Span>{{1, 3}});
    CHECK(PlacementFrames(animation) == std::vector<uint32_t>{1, 3});
    CHECK(EndFrames(animation) == std::vector<uint16_t>{4});
}

TEST_CASE("Lengthening a span's end moves its remove and stops at another span") {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, kDepth, 1, 3);
    Add(animation, kDepth, 7, 8);
    REQUIRE(Document::TrimSpan(animation, kRoot, kDepth, 1, {1, 6}).has_value());
    CHECK(SpansOf(animation, kDepth) == std::vector<Document::Span>{{1, 6}, {7, 8}});
    CHECK(EndFrames(animation) == std::vector<uint16_t>{7, 9});

    const AfpAnimation::Animation before = animation;
    CHECK_FALSE(Document::TrimSpan(animation, kRoot, kDepth, 1, {1, 7}).has_value());
    CHECK_FALSE(Document::TrimSpan(animation, kRoot, kDepth, 1, {1, 10}).has_value());
    CHECK_FALSE(Document::TrimSpan(animation, kRoot, kDepth, 1, {4, 2}).has_value());
    CHECK(animation == before);
}

TEST_CASE("A span's start moves earlier into free frames only") {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, kDepth, 0, 1);
    Add(animation, kDepth, 4, 6);
    REQUIRE(Document::TrimSpan(animation, kRoot, kDepth, 5, {2, 6}).has_value());
    CHECK(SpansOf(animation, kDepth) == std::vector<Document::Span>{{0, 1}, {2, 6}});
    CHECK_FALSE(Document::TrimSpan(animation, kRoot, kDepth, 5, {1, 6}).has_value());
}

TEST_CASE("Moving a span's start later folds the skipped updates into its first placement") {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, kDepth, 1, 7);
    AfpAnimation::Placement moved = Update(kUseMatrix);
    moved.translation = std::array<int32_t, 2>{200, 0};
    moved.scale = std::array<int32_t, 2>{2048, 2048};
    Document::InsertTag(animation.root, 2, AfpAnimation::Tag{moved});
    AfpAnimation::Placement tinted = Update(kUseColour);
    tinted.multiply_colour = std::array<int16_t, 4>{0, 255, 255, 255};
    tinted.character = uint16_t{9};
    Document::InsertTag(animation.root, 3, AfpAnimation::Tag{tinted});
    AfpAnimation::Placement later = Update(kUseMatrix);
    later.translation = std::array<int32_t, 2>{400, 0};
    Document::InsertTag(animation.root, 5, AfpAnimation::Tag{later});
    const auto shown = Document::ReplayDepth(animation.root, kDepth, 4, 7);

    const auto trimmed =
        Document::TrimSpan(animation, kRoot, kDepth, 1, {.first_frame = 4, .last_frame = 7});
    INFO(Error(trimmed));
    REQUIRE(trimmed.has_value());
    CHECK(SpansOf(animation, kDepth) == std::vector<Document::Span>{{4, 7}});
    CHECK(PlacementFrames(animation) == std::vector<uint32_t>{4, 5});
    const AfpAnimation::Placement& create = CreateOf(animation);
    CHECK(create.translation == std::array<int32_t, 2>{200, 0});
    CHECK(create.character == uint16_t{9});
    CHECK(create.multiply_colour == std::array<int16_t, 4>{0, 255, 255, 255});
    CHECK((create.flags & kUseColour) != 0);
    CHECK(Document::ReplayDepth(animation.root, kDepth, 4, 7) == shown);
}

TEST_CASE("Moving a 3D span's start later keeps the translation, depth and 3D matrix it reached") {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, kDepth, 1, 7);
    auto& create = std::get<AfpAnimation::Placement>(animation.root.tags.at(0).body);
    create.flags |= kThreeD;
    create.translation = std::array<int32_t, 2>{10, 10};
    create.scale = std::array<int32_t, 2>{2048, 2048};
    create.matrix_3d = std::array<int32_t, 9>{1024, 0, 0, 0, 1024, 0, 0, 0, 1024};
    AfpAnimation::Placement moved = Update(kUseMatrix | kThreeD);
    moved.translation = std::array<int32_t, 2>{100, 0};
    Document::InsertTag(animation.root, 2, AfpAnimation::Tag{moved});
    AfpAnimation::Placement deeper = Update(kThreeD);
    deeper.translation_z = int32_t{50};
    Document::InsertTag(animation.root, 3, AfpAnimation::Tag{deeper});
    AfpAnimation::Placement turned = Update(kThreeD);
    turned.matrix_3d = std::array<int32_t, 9>{0, 1024, 0, -1024, 0, 0, 0, 0, 1024};
    Document::InsertTag(animation.root, 4, AfpAnimation::Tag{turned});
    AfpAnimation::Placement recentred = Update(kUseMatrix | kThreeD);
    Document::InsertTag(animation.root, 5, AfpAnimation::Tag{recentred});
    AfpAnimation::Placement later = Update(kUseMatrix | kThreeD);
    later.translation = std::array<int32_t, 2>{300, 0};
    Document::InsertTag(animation.root, 6, AfpAnimation::Tag{later});
    const auto shown = Document::ReplayDepth(animation.root, kDepth, 6, 7);

    const auto trimmed =
        Document::TrimSpan(animation, kRoot, kDepth, 1, {.first_frame = 6, .last_frame = 7});
    INFO(Error(trimmed));
    REQUIRE(trimmed.has_value());
    CHECK(PlacementFrames(animation) == std::vector<uint32_t>{6, 6});
    const AfpAnimation::Placement& folded = CreateOf(animation);
    CHECK((folded.flags & kThreeD) != 0);
    CHECK_FALSE(folded.translation.has_value());
    CHECK(folded.scale == std::array<int32_t, 2>{2048, 2048});
    CHECK(folded.translation_z == int32_t{50});
    CHECK(folded.matrix_3d == std::array<int32_t, 9>{0, 1024, 0, -1024, 0, 0, 0, 0, 1024});
    CHECK(Document::ReplayDepth(animation.root, kDepth, 6, 7) == shown);
}

TEST_CASE("Moving a span's start later keeps the newest curve in every slot") {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, kDepth, 1, 7);
    std::get<AfpAnimation::Placement>(animation.root.tags.at(0).body).curves =
        std::vector<AfpAnimation::Curve>{Curve(0, 2, 1), Curve(1, 3, 1)};
    Document::InsertTag(animation.root, 2, AfpAnimation::Tag{Curved({Curve(1, 3, 2)})});
    Document::InsertTag(animation.root, 3, AfpAnimation::Tag{Curved({Curve(0, 1, 3)})});
    Document::InsertTag(animation.root, 5, AfpAnimation::Tag{Curved({Curve(0, 1, 4)})});

    const auto trimmed =
        Document::TrimSpan(animation, kRoot, kDepth, 1, {.first_frame = 4, .last_frame = 7});
    INFO(Error(trimmed));
    REQUIRE(trimmed.has_value());
    CHECK(CreateOf(animation).curves ==
          std::vector<AfpAnimation::Curve>{Curve(0, 1, 3), Curve(1, 3, 2)});
    CHECK(PlacementFrames(animation) == std::vector<uint32_t>{4, 5});
}

TEST_CASE("A trim whose folded curves leave a kept update no room is refused") {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, kDepth, 1, 7);
    std::get<AfpAnimation::Placement>(animation.root.tags.at(0).body).curves =
        std::vector<AfpAnimation::Curve>{Curve(0, 3, 1)};
    Document::InsertTag(animation.root, 2, AfpAnimation::Tag{Curved({Curve(0, 1, 2)})});
    Document::InsertTag(animation.root, 5, AfpAnimation::Tag{Curved({Curve(0, 3, 3)})});
    const AfpAnimation::Animation before = animation;
    CHECK_FALSE(Document::TrimSpan(animation, kRoot, kDepth, 1, {.first_frame = 4, .last_frame = 7})
                    .has_value());
    CHECK(animation == before);
}

TEST_CASE("A span that switches between 2D and 3D is not folded") {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, kDepth, 1, 7);
    Document::InsertTag(animation.root, 2, AfpAnimation::Tag{Update(kUseMatrix | kThreeD)});
    const AfpAnimation::Animation before = animation;
    CHECK_FALSE(Document::TrimSpan(animation, kRoot, kDepth, 1, {.first_frame = 3, .last_frame = 7})
                    .has_value());
    CHECK(animation == before);
}

TEST_CASE("An owned depth's keyframes are cut to the trimmed range and keep their values") {
    Document::AuthoredDepth authored{
        .animation = "afp/a",
        .depth = kDepth,
        .first_frame = 0,
        .last_frame = 8,
        .tracks = {Document::Track{.property = "Ratio",
                                   .keys = {Key(0, 0, Document::Ease::Linear),
                                            Key(4, 40, Document::Ease::Linear),
                                            Key(8, 80, Document::Ease::Linear)}},
                   Document::Track{.property = "Character",
                                   .keys = {Key(0, 7, Document::Ease::Hold)}}},
        .script = {},
        .clip = {}};
    REQUIRE(Document::TrimAuthored(authored, {2, 6}).has_value());
    CHECK(authored.first_frame == 2);
    CHECK(authored.last_frame == 6);
    CHECK(authored.tracks[0].keys ==
          std::vector<Document::Keyframe>{Key(2, 20, Document::Ease::Linear),
                                          Key(4, 40, Document::Ease::Linear),
                                          Key(6, 60, Document::Ease::Hold)});
    CHECK(authored.tracks[1].keys ==
          std::vector<Document::Keyframe>{Key(2, 7, Document::Ease::Hold)});

    REQUIRE(Document::TrimAuthored(authored, {1, 9}).has_value());
    CHECK(authored.tracks[0].keys.size() == 3);
    CHECK_FALSE(Document::TrimAuthored(authored, {5, 4}).has_value());
}

TEST_CASE("An owned span is trimmed and rewritten from its trimmed keyframes") {
    AfpAnimation::Animation animation = Clip(10);
    Add(animation, kDepth, 1, 7);
    for (uint32_t frame = 2; frame <= 7; frame++) {
        AfpAnimation::Placement moved = Update(kUseMatrix);
        moved.translation = std::array<int32_t, 2>{static_cast<int32_t>(frame) * 20, 0};
        moved.end_frame = 8;
        Document::InsertTag(animation.root, frame, AfpAnimation::Tag{moved});
    }
    auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 1);
    INFO(Error(owned));
    REQUIRE(owned.has_value());
    if (!owned) return;
    const auto shown = Document::ReplayDepth(animation.root, kDepth, 3, 5);

    Document::AuthoredDepth authored = owned->authored;
    const auto trimmed =
        Document::TrimOwnedSpan(animation, authored, {.first_frame = 3, .last_frame = 5});
    INFO(Error(trimmed));
    REQUIRE(trimmed.has_value());
    CHECK(authored.first_frame == 3);
    CHECK(authored.last_frame == 5);
    CHECK(SpansOf(animation, kDepth) == std::vector<Document::Span>{{3, 5}});
    CHECK(Document::ReplayDepth(animation.root, kDepth, 3, 5) == shown);
    const auto again = Document::BakedFor(animation, authored);
    REQUIRE(again.has_value());

    const AfpAnimation::Animation before = animation;
    const Document::AuthoredDepth kept = authored;
    CHECK_FALSE(Document::TrimOwnedSpan(animation, authored, {3, 20}).has_value());
    CHECK(animation == before);
    CHECK(authored == kept);
}
