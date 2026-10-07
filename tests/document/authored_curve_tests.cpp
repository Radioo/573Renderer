#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/clip.h"
#include "document/curve_values.h"
#include "document/keyframes.h"
#include "document/tags.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr uint32_t kUpdateExisting = 0x1;
constexpr uint32_t kUseMatrix = 0x4;
constexpr uint16_t kIntValues = 0x1;
constexpr uint16_t kControlPoints = 0x10;
constexpr uint16_t kDepth = 3;
const Document::ClipId kRoot{};

std::vector<AfpAnimation::Curve> Bend(int32_t by) {
    return {AfpAnimation::Curve{.slot = 0, .flags = 0, .values = {0, by, 100, by}},
            AfpAnimation::Curve{.slot = 1, .flags = kIntValues, .values = {0, 70000, 1, 2, 3, by}}};
}

std::vector<AfpAnimation::Curve> Reshaped() {
    return {AfpAnimation::Curve{
        .slot = 1, .flags = kIntValues | kControlPoints, .values = {1, 2, 3, 4, 5, 6}}};
}

AfpAnimation::Placement Update(std::optional<std::vector<AfpAnimation::Curve>> curves, int32_t x) {
    AfpAnimation::Placement placement;
    placement.flags = kUpdateExisting | kUseMatrix;
    placement.depth = kDepth;
    placement.end_frame = 5;
    if (curves) placement.extended_flags = 0U;
    placement.curves = std::move(curves);
    placement.translation = std::array<int32_t, 2>{x, 0};
    return placement;
}

AfpAnimation::Animation Curved(std::optional<std::vector<AfpAnimation::Curve>> later) {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    for (int i = 0; i < 5; i++)
        animation.root.frames.push_back(AfpAnimation::Frame{.first_tag = 0, .tag_count = 0});
    AfpAnimation::Placement create;
    create.flags = kUseMatrix;
    create.depth = kDepth;
    create.end_frame = 5;
    create.character = uint16_t{7};
    create.curves = Bend(10);
    create.extended_flags = 0U;
    create.translation = std::array<int32_t, 2>{0, 0};
    Document::InsertTag(animation.root, 0, AfpAnimation::Tag{create});
    Document::InsertTag(animation.root, 1, AfpAnimation::Tag{Update(std::nullopt, 20)});
    Document::InsertTag(animation.root, 2, AfpAnimation::Tag{Update(std::move(later), 40)});
    return animation;
}

bool Accepted(const std::vector<int64_t>& numbers) {
    return Document::CurvesFrom(numbers).has_value();
}

const Document::Track* TrackFor(const Document::AuthoredDepth& authored,
                                const std::string& property) {
    const auto found = std::ranges::find(authored.tracks, property, &Document::Track::property);
    return found == authored.tracks.end() ? nullptr : &*found;
}

}

TEST_CASE("Curves turn into numbers and back unchanged") {
    const std::vector<AfpAnimation::Curve> curves = Bend(-5);
    const auto back = Document::CurvesFrom(Document::CurveNumbers(curves));
    REQUIRE(back.has_value());
    if (!back) return;
    CHECK(*back == curves);
    const auto none = Document::CurvesFrom(Document::CurveNumbers({}));
    REQUIRE(none.has_value());
    if (!none) return;
    CHECK(none->empty());
}

TEST_CASE("Numbers the curve field could not hold are refused") {
    CHECK_FALSE(Accepted({}));
    CHECK_FALSE(Accepted({1, 32, 0, 2, 0, 0}));
    CHECK_FALSE(Accepted({2, 4, 0, 2, 0, 0, 3, 0, 2, 0, 0}));
    CHECK_FALSE(Accepted({1, 4, 0, 3, 0, 0, 0}));
    CHECK_FALSE(Accepted({1, 4, 0, 2, 0, 40000}));
    CHECK_FALSE(Accepted({1, 4, 0, 2, 0, 0, 7}));
    CHECK(Accepted({1, 4, kIntValues, 2, 0, 40000}));
}

TEST_CASE("A depth whose updates change its curves is owned with a stepped curve track") {
    const AfpAnimation::Animation animation = Curved(Bend(30));
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    const std::string error = owned.has_value() ? std::string() : owned.error();
    INFO(error);
    REQUIRE(owned.has_value());
    if (!owned) return;
    const Document::Track* curves = TrackFor(owned->authored, "Curves");
    REQUIRE(curves != nullptr);
    REQUIRE(curves->keys.size() == 2);
    CHECK(curves->keys[0].frame == 0);
    CHECK(curves->keys[1].frame == 2);
    CHECK(curves->keys[1].ease == Document::Ease::Hold);
    CHECK_FALSE(owned->baked.create.curves.has_value());

    AfpAnimation::Animation written = animation;
    REQUIRE(Document::WriteAuthored(written, owned->authored, owned->baked).has_value());
    CHECK(written == animation);
}

TEST_CASE("A depth whose updates replace only some curves is written back exactly") {
    const AfpAnimation::Animation animation = Curved(Reshaped());
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    const std::string error = owned.has_value() ? std::string() : owned.error();
    INFO(error);
    REQUIRE(owned.has_value());
    if (!owned) return;
    AfpAnimation::Animation written = animation;
    REQUIRE(Document::WriteAuthored(written, owned->authored, owned->baked).has_value());
    CHECK(written == animation);
}

TEST_CASE("A depth whose updates leave its curves alone keeps them on the create") {
    const AfpAnimation::Animation animation = Curved(std::nullopt);
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    if (!owned) return;
    CHECK(TrackFor(owned->authored, "Curves") == nullptr);
    CHECK(owned->baked.create.curves == Bend(10));
    AfpAnimation::Animation written = animation;
    REQUIRE(Document::WriteAuthored(written, owned->authored, owned->baked).has_value());
    CHECK(written == animation);
}

TEST_CASE("A curve key the first set's controller could not hold is refused") {
    const AfpAnimation::Animation animation = Curved(Bend(30));
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    if (!owned) return;
    const auto refused = [&](const std::vector<AfpAnimation::Curve>& later) {
        Document::AuthoredDepth authored = owned->authored;
        const auto track =
            std::ranges::find(authored.tracks, std::string("Curves"), &Document::Track::property);
        track->keys[1].value = Document::CurveNumbers(later);
        AfpAnimation::Animation written = animation;
        return !Document::WriteAuthored(written, authored, owned->baked).has_value();
    };
    CHECK_FALSE(refused(Reshaped()));
    CHECK(refused({AfpAnimation::Curve{.slot = 0, .flags = 0, .values = {0, 1, 2, 3, 4, 5}}}));
    CHECK(refused({AfpAnimation::Curve{.slot = 2, .flags = 0, .values = {0, 1}}}));
    CHECK(refused({AfpAnimation::Curve{
        .slot = 1, .flags = kControlPoints, .values = std::vector<int32_t>(24, 1)}}));
}

TEST_CASE("A first curve set with a gap in its slots is refused") {
    const AfpAnimation::Animation animation = Curved(Bend(30));
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    REQUIRE(owned.has_value());
    if (!owned) return;
    Document::AuthoredDepth authored = owned->authored;
    const auto track =
        std::ranges::find(authored.tracks, std::string("Curves"), &Document::Track::property);
    REQUIRE(track != authored.tracks.end());
    std::vector<AfpAnimation::Curve> gapped = Bend(10);
    gapped[1].slot = 2;
    track->keys[0].value = Document::CurveNumbers(gapped);
    track->keys[1].value = Document::CurveNumbers({gapped[0]});
    AfpAnimation::Animation written = animation;
    CHECK_FALSE(Document::WriteAuthored(written, authored, owned->baked).has_value());
}

TEST_CASE("A curve key moved onto a frame without an extended word gets one") {
    const AfpAnimation::Animation animation = Curved(Bend(30));
    const auto owned = Document::OwnDepth(animation, kRoot, "afp/a", kDepth, 0);
    const std::string error = owned.has_value() ? std::string() : owned.error();
    INFO(error);
    REQUIRE(owned.has_value());
    if (!owned) return;
    Document::AuthoredDepth authored = owned->authored;
    const auto track =
        std::ranges::find(authored.tracks, std::string("Curves"), &Document::Track::property);
    REQUIRE(track != authored.tracks.end());
    track->keys[1].frame = 1;
    AfpAnimation::Animation written = animation;
    REQUIRE(Document::WriteAuthored(written, authored, owned->baked).has_value());
    const auto& moved = std::get<AfpAnimation::Placement>(written.root.tags.at(1).body);
    CHECK(moved.curves.has_value());
    CHECK(moved.extended_flags == 0U);
    const auto& left = std::get<AfpAnimation::Placement>(written.root.tags.at(2).body);
    CHECK_FALSE(left.curves.has_value());
    CHECK_FALSE(left.extended_flags.has_value());
    CHECK(AfpAnimation::Write(written).has_value());
}
