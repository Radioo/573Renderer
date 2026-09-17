#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/keyframes.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

Document::Keyframe Key(uint32_t frame, std::vector<int64_t> value) {
    return Document::Keyframe{
        .frame = frame, .value = std::move(value), .ease = Document::Ease::Linear, .bezier = {}};
}

Document::Track Two(int64_t first, int64_t second, Document::Ease ease) {
    return Document::Track{
        .property = "Translation",
        .keys = {
            Document::Keyframe{.frame = 10, .value = {first, 0}, .ease = ease, .bezier = {}},
            Document::Keyframe{.frame = 20, .value = {second, 100}, .ease = ease, .bezier = {}}}};
}

std::vector<int64_t> Sampled(const Document::Track& track, uint32_t frame) {
    return Document::SampleTrack(track, frame);
}

}

TEST_CASE("A property with one keyframe holds that value across the whole range") {
    const Document::Track track{.property = "Scale", .keys = {Key(5, {1024, 2048})}};
    CHECK(Sampled(track, 0) == std::vector<int64_t>{1024, 2048});
    CHECK(Sampled(track, 5) == std::vector<int64_t>{1024, 2048});
    CHECK(Sampled(track, 900) == std::vector<int64_t>{1024, 2048});
}

TEST_CASE("Sampling outside the keyframes holds the nearest one") {
    const Document::Track track = Two(0, 1000, Document::Ease::Linear);
    CHECK(Sampled(track, 0) == std::vector<int64_t>{0, 0});
    CHECK(Sampled(track, 9) == std::vector<int64_t>{0, 0});
    CHECK(Sampled(track, 20) == std::vector<int64_t>{1000, 100});
    CHECK(Sampled(track, 500) == std::vector<int64_t>{1000, 100});
}

TEST_CASE("A linear ease walks every component evenly") {
    const Document::Track track = Two(0, 1000, Document::Ease::Linear);
    CHECK(Sampled(track, 10) == std::vector<int64_t>{0, 0});
    CHECK(Sampled(track, 15) == std::vector<int64_t>{500, 50});
    CHECK(Sampled(track, 12) == std::vector<int64_t>{200, 20});
    CHECK(Sampled(track, 19) == std::vector<int64_t>{900, 90});
}

TEST_CASE("A hold ease keeps the earlier value until the next keyframe") {
    const Document::Track track = Two(0, 1000, Document::Ease::Hold);
    CHECK(Sampled(track, 10) == std::vector<int64_t>{0, 0});
    CHECK(Sampled(track, 15) == std::vector<int64_t>{0, 0});
    CHECK(Sampled(track, 19) == std::vector<int64_t>{0, 0});
    CHECK(Sampled(track, 20) == std::vector<int64_t>{1000, 100});
}

TEST_CASE("A bezier ease with its control points on the diagonal is linear") {
    Document::Track track = Two(0, 1000, Document::Ease::Bezier);
    for (Document::Keyframe& key : track.keys) {
        key.bezier =
            Document::Bezier{.x1 = 1.0 / 3.0, .y1 = 1.0 / 3.0, .x2 = 2.0 / 3.0, .y2 = 2.0 / 3.0};
    }
    CHECK(Sampled(track, 15) == std::vector<int64_t>{500, 50});
    CHECK(Sampled(track, 12) == std::vector<int64_t>{200, 20});
}

TEST_CASE("A bezier ease that starts slowly lands on the numbers the curve gives") {
    Document::Track track = Two(0, 1000, Document::Ease::Bezier);
    track.keys.front().bezier = Document::Bezier{.x1 = 0.42, .y1 = 0.0, .x2 = 1.0, .y2 = 1.0};
    CHECK(Sampled(track, 12) == std::vector<int64_t>{62, 6});
    CHECK(Sampled(track, 15) == std::vector<int64_t>{315, 32});
    CHECK(Sampled(track, 18) == std::vector<int64_t>{692, 69});
    CHECK(Sampled(track, 10) == std::vector<int64_t>{0, 0});
    CHECK(Sampled(track, 20) == std::vector<int64_t>{1000, 100});
}

TEST_CASE("A bezier ease that is symmetric is halfway at the halfway frame") {
    Document::Track track = Two(0, 1000, Document::Ease::Bezier);
    track.keys.front().bezier = Document::Bezier{.x1 = 0.42, .y1 = 0.0, .x2 = 0.58, .y2 = 1.0};
    CHECK(Sampled(track, 15) == std::vector<int64_t>{500, 50});
}

TEST_CASE("Sampling the same track twice gives the same numbers") {
    Document::Track track = Two(-500, 500, Document::Ease::Bezier);
    track.keys.front().bezier = Document::Bezier{.x1 = 0.17, .y1 = 0.67, .x2 = 0.83, .y2 = 0.67};
    for (uint32_t frame = 0; frame < 30; frame++)
        CHECK(Sampled(track, frame) == Sampled(track, frame));
}

TEST_CASE("A sampled value rounds away from zero on both sides") {
    const Document::Track up{.property = "Translation", .keys = {Key(0, {0}), Key(2, {1})}};
    CHECK(Sampled(up, 1) == std::vector<int64_t>{1});
    const Document::Track down{.property = "Translation", .keys = {Key(0, {0}), Key(2, {-1})}};
    CHECK(Sampled(down, 1) == std::vector<int64_t>{-1});
}

TEST_CASE("Keyframes go in wherever they belong in time") {
    Document::Track track{.property = "Translation", .keys = {Key(10, {0, 0})}};
    REQUIRE(Document::AddKeyframe(track, Key(30, {300, 0})).has_value());
    REQUIRE(Document::AddKeyframe(track, Key(20, {200, 0})).has_value());
    REQUIRE(track.keys.size() == 3);
    CHECK(track.keys[0].frame == 10);
    CHECK(track.keys[1].frame == 20);
    CHECK(track.keys[2].frame == 30);
    CHECK(Document::CheckTrack(track).has_value());
}

TEST_CASE("A keyframe that would break the track is refused") {
    Document::Track track{.property = "Translation", .keys = {Key(10, {0, 0})}};
    CHECK_FALSE(Document::AddKeyframe(track, Key(10, {1, 1})).has_value());
    CHECK_FALSE(Document::AddKeyframe(track, Key(20, {1})).has_value());
    CHECK_FALSE(Document::AddKeyframe(track, Key(20, {})).has_value());
    CHECK_FALSE(
        Document::AddKeyframe(
            track,
            {.frame = 20, .value = {1, 1}, .ease = Document::Ease::Bezier, .bezier = {.x1 = -1.0}})
            .has_value());
    CHECK(track.keys.size() == 1);
}

TEST_CASE("A keyframe can be given a new value and a new ease") {
    Document::Track track = Two(0, 1000, Document::Ease::Linear);
    REQUIRE(Document::SetKeyframeValue(track, 20, {2000, 200}).has_value());
    CHECK(Sampled(track, 20) == std::vector<int64_t>{2000, 200});

    REQUIRE(Document::SetKeyframeEase(track, 10, Document::Ease::Hold, {}).has_value());
    CHECK(Sampled(track, 15) == std::vector<int64_t>{0, 0});
    CHECK(Document::CheckTrack(track).has_value());
}

TEST_CASE("An edit to a keyframe that is not there is refused") {
    Document::Track track = Two(0, 1000, Document::Ease::Linear);
    CHECK_FALSE(Document::SetKeyframeValue(track, 11, {1, 1}).has_value());
    CHECK_FALSE(Document::SetKeyframeValue(track, 10, {1}).has_value());
    CHECK_FALSE(Document::SetKeyframeEase(track, 11, Document::Ease::Hold, {}).has_value());
    CHECK_FALSE(Document::RemoveKeyframe(track, 11).has_value());
    CHECK(track == Two(0, 1000, Document::Ease::Linear));
}

TEST_CASE("A track keeps its last keyframe") {
    Document::Track track = Two(0, 1000, Document::Ease::Linear);
    REQUIRE(Document::RemoveKeyframe(track, 10).has_value());
    CHECK(track.keys.size() == 1);
    CHECK_FALSE(Document::RemoveKeyframe(track, 20).has_value());
    CHECK(track.keys.size() == 1);
}

TEST_CASE("A track that cannot be sampled says why") {
    CHECK_FALSE(Document::CheckTrack({.property = "", .keys = {}}).has_value());
    CHECK_FALSE(Document::CheckTrack({.property = "Translation", .keys = {}}).has_value());
    CHECK_FALSE(
        Document::CheckTrack({.property = "Translation", .keys = {Key(0, {})}}).has_value());
    CHECK_FALSE(
        Document::CheckTrack({.property = "Translation", .keys = {Key(0, {1, 2}), Key(1, {1})}})
            .has_value());
    CHECK_FALSE(
        Document::CheckTrack({.property = "Translation", .keys = {Key(5, {1}), Key(5, {2})}})
            .has_value());
    CHECK(Document::CheckTrack({.property = "Translation", .keys = {Key(0, {1}), Key(1, {2})}})
              .has_value());
}

TEST_CASE("Ease names round trip") {
    for (const Document::Ease ease :
         {Document::Ease::Hold, Document::Ease::Linear, Document::Ease::Bezier}) {
        const std::string name{Document::EaseName(ease)};
        const auto back = Document::EaseFor(name);
        REQUIRE(back.has_value());
        CHECK(*back == ease);
    }
    CHECK_FALSE(Document::EaseFor("spring").has_value());
    CHECK_FALSE(Document::EaseFor("").has_value());
}

TEST_CASE("The ease a curve editor draws is the one the track samples") {
    const Document::Bezier bezier{.x1 = 0.2, .y1 = 0.9, .x2 = 0.4, .y2 = 1.2};
    Document::Track track{.property = "Translation", .keys = {Key(0, {0}), Key(10, {10000})}};
    track.keys[0].ease = Document::Ease::Bezier;
    track.keys[0].bezier = bezier;
    for (uint32_t frame = 0; frame <= 10; frame++) {
        const double progress =
            Document::EaseProgress(Document::Ease::Bezier, bezier, frame / 10.0);
        CHECK(Sampled(track, frame) == std::vector<int64_t>{std::llround(progress * 10000.0)});
    }
    CHECK(Document::EaseProgress(Document::Ease::Hold, {}, 0.7) == 0.0);
    CHECK(Document::EaseProgress(Document::Ease::Linear, {}, 0.7) == 0.7);
}

TEST_CASE("A dragged ease handle stays inside the segment's time") {
    const Document::Bezier kept =
        Document::WithinTime({.x1 = -0.5, .y1 = -2.0, .x2 = 1.5, .y2 = 3.0});
    CHECK(kept == Document::Bezier{.x1 = 0.0, .y1 = -2.0, .x2 = 1.0, .y2 = 3.0});
}

TEST_CASE("Every ease preset is a curve a keyframe accepts") {
    REQUIRE(!Document::EasePresets().empty());
    for (const Document::EasePreset& preset : Document::EasePresets()) {
        Document::Track track = Two(0, 1000, Document::Ease::Linear);
        INFO(preset.name);
        CHECK(Document::SetKeyframeEase(track, 10, Document::Ease::Bezier, preset.bezier)
                  .has_value());
        CHECK(std::abs(Document::EaseProgress(Document::Ease::Bezier, preset.bezier, 0.0)) < 1e-9);
        CHECK(std::abs(Document::EaseProgress(Document::Ease::Bezier, preset.bezier, 1.0) - 1.0) <
              1e-9);
    }
}

TEST_CASE("A stepped track's keyframes may hold different numbers of values") {
    Document::Track filters{.property = "Filters", .keys = {Key(0, {0})}};
    filters.keys[0].ease = Document::Ease::Hold;
    Document::Keyframe two = Key(4, {1, 0, 6, 0, 0, 0});
    two.ease = Document::Ease::Hold;
    REQUIRE(Document::AddKeyframe(filters, two).has_value());
    CHECK(Document::CheckTrack(filters).has_value());
    CHECK(Sampled(filters, 3) == std::vector<int64_t>{0});
    CHECK(Sampled(filters, 4) == std::vector<int64_t>{1, 0, 6, 0, 0, 0});
    REQUIRE(Document::SetKeyframeValue(filters, 0, {1, 2, 9}).has_value());
    CHECK(Sampled(filters, 0) == std::vector<int64_t>{1, 2, 9});

    Document::Track scale{.property = "Scale", .keys = {Key(0, {1024, 1024})}};
    CHECK_FALSE(Document::AddKeyframe(scale, Key(4, {1024})).has_value());
    CHECK_FALSE(Document::SetKeyframeValue(scale, 0, {1024}).has_value());
    scale.keys.push_back(Key(4, {1024}));
    CHECK_FALSE(Document::CheckTrack(scale).has_value());
}
