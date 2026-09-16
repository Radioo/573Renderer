#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/keyframe_edit.h"
#include "document/keyframes.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

Document::AuthoredDepth Depth() {
    Document::Track track{.property = "Translation", .keys = {}};
    track.keys.push_back(Document::Keyframe{
        .frame = 0, .value = {0, 0}, .ease = Document::Ease::Linear, .bezier = {}});
    track.keys.push_back(Document::Keyframe{
        .frame = 8, .value = {800, 400}, .ease = Document::Ease::Linear, .bezier = {}});
    return Document::AuthoredDepth{.animation = "afp/scene",
                                   .depth = 1,
                                   .first_frame = 0,
                                   .last_frame = 8,
                                   .tracks = {track},
                                   .script = std::nullopt,
                                   .clip = {}};
}

const Document::Track& TrackOf(const Document::AuthoredDepth& depth, const std::string& property) {
    const auto found = std::ranges::find(depth.tracks, property, &Document::Track::property);
    REQUIRE(found != depth.tracks.end());
    return *found;
}

std::vector<std::vector<int64_t>> Sampled(const Document::AuthoredDepth& depth) {
    std::vector<std::vector<int64_t>> out;
    const Document::Track& track = TrackOf(depth, "Translation");
    for (uint32_t frame = depth.first_frame; frame <= depth.last_frame; frame++)
        out.push_back(Document::SampleTrack(track, frame));
    return out;
}

}

TEST_CASE("A keyframe added on a linear segment changes nothing that is sampled") {
    Document::AuthoredDepth depth = Depth();
    const std::vector<std::vector<int64_t>> before = Sampled(depth);

    const auto added = Document::AddKeyAt(depth, "Translation", 4);
    REQUIRE(added.has_value());
    CHECK(TrackOf(depth, "Translation").keys.size() == 3);
    CHECK(Sampled(depth) == before);
}

TEST_CASE("A keyframe added on a held segment changes nothing that is sampled") {
    Document::AuthoredDepth depth = Depth();
    REQUIRE(Document::SetKeyEaseAt(depth, "Translation", 0, Document::Ease::Hold, {}).has_value());
    const std::vector<std::vector<int64_t>> before = Sampled(depth);

    REQUIRE(Document::AddKeyAt(depth, "Translation", 5).has_value());
    CHECK(Sampled(depth) == before);
    CHECK(TrackOf(depth, "Translation").keys[1].ease == Document::Ease::Hold);
}

TEST_CASE("A keyframe cannot be added outside the authored range") {
    Document::AuthoredDepth depth = Depth();
    const auto added = Document::AddKeyAt(depth, "Translation", 9);
    REQUIRE_FALSE(added.has_value());
    CHECK(added.error().find("9") != std::string::npos);
}

TEST_CASE("A keyframe cannot be added where one already is") {
    Document::AuthoredDepth depth = Depth();
    CHECK_FALSE(Document::AddKeyAt(depth, "Translation", 8).has_value());
}

TEST_CASE("A property nothing animates is not a property that can be keyed") {
    Document::AuthoredDepth depth = Depth();
    const auto added = Document::AddKeyAt(depth, "Rotate skew", 4);
    REQUIRE_FALSE(added.has_value());
    CHECK(added.error().find("Rotate skew") != std::string::npos);
}

TEST_CASE("A keyframe moves to another frame inside the range") {
    Document::AuthoredDepth depth = Depth();
    REQUIRE(Document::AddKeyAt(depth, "Translation", 4).has_value());
    REQUIRE(Document::MoveKeyTo(depth, "Translation", 4, 6).has_value());

    const Document::Track& track = TrackOf(depth, "Translation");
    REQUIRE(track.keys.size() == 3);
    CHECK(track.keys[1].frame == 6);
    CHECK(std::ranges::is_sorted(track.keys, {}, &Document::Keyframe::frame));
}

TEST_CASE("A keyframe cannot be moved outside the authored range") {
    Document::AuthoredDepth depth = Depth();
    CHECK_FALSE(Document::MoveKeyTo(depth, "Translation", 8, 12).has_value());
    CHECK(TrackOf(depth, "Translation").keys.back().frame == 8);
}

TEST_CASE("A keyframe value is set where the keyframe is") {
    Document::AuthoredDepth depth = Depth();
    REQUIRE(Document::SetKeyValueAt(depth, "Translation", 8, "1600, 0").has_value());
    CHECK(TrackOf(depth, "Translation").keys.back().value == std::vector<int64_t>{1600, 0});
    CHECK(Document::SampleTrack(TrackOf(depth, "Translation"), 4) == std::vector<int64_t>{800, 0});
}

TEST_CASE("A keyframe value has to hold what the track keys") {
    Document::AuthoredDepth depth = Depth();
    CHECK_FALSE(Document::SetKeyValueAt(depth, "Translation", 8, "1600").has_value());
    CHECK_FALSE(Document::SetKeyValueAt(depth, "Translation", 8, "1600, left").has_value());
    CHECK(TrackOf(depth, "Translation").keys.back().value == std::vector<int64_t>{800, 400});
}

TEST_CASE("A frame with no keyframe holds no value to set") {
    Document::AuthoredDepth depth = Depth();
    CHECK_FALSE(Document::SetKeyValueAt(depth, "Translation", 4, "1, 2").has_value());
}

TEST_CASE("A keyframe is looked up by its property and frame") {
    const Document::AuthoredDepth depth = Depth();
    const std::optional<Document::Keyframe> key = Document::KeyAt(depth, "Translation", 8);
    REQUIRE(key.has_value());
    CHECK(Document::KeyValueText(*key) == "800, 400");
    CHECK_FALSE(Document::KeyAt(depth, "Translation", 4).has_value());
    CHECK_FALSE(Document::KeyAt(depth, "Multiply colour", 0).has_value());
}

TEST_CASE("A keyframe takes a bezier ease and its control points") {
    Document::AuthoredDepth depth = Depth();
    const Document::Bezier curve{.x1 = 0.25, .y1 = 0.1, .x2 = 0.25, .y2 = 1.0};
    REQUIRE(
        Document::SetKeyEaseAt(depth, "Translation", 0, Document::Ease::Bezier, curve).has_value());

    const Document::Track& track = TrackOf(depth, "Translation");
    CHECK(track.keys.front().ease == Document::Ease::Bezier);
    CHECK(track.keys.front().bezier == curve);
}

TEST_CASE("An ease control point outside its range is refused") {
    Document::AuthoredDepth depth = Depth();
    const Document::Bezier curve{.x1 = 2.0, .y1 = 0.0, .x2 = 0.5, .y2 = 1.0};
    CHECK_FALSE(
        Document::SetKeyEaseAt(depth, "Translation", 0, Document::Ease::Bezier, curve).has_value());
}

TEST_CASE("A keyframe is removed by the frame it is on") {
    Document::AuthoredDepth depth = Depth();
    REQUIRE(Document::AddKeyAt(depth, "Translation", 4).has_value());
    REQUIRE(Document::RemoveKeyAt(depth, "Translation", 4).has_value());

    const Document::Track& track = TrackOf(depth, "Translation");
    REQUIRE(track.keys.size() == 2);
    CHECK(track.keys.front().frame == 0);
    CHECK(track.keys.back().frame == 8);
}

TEST_CASE("Removing the last keyframe of a property is refused") {
    Document::AuthoredDepth depth = Depth();
    REQUIRE(Document::RemoveKeyAt(depth, "Translation", 0).has_value());
    const auto last = Document::RemoveKeyAt(depth, "Translation", 8);
    REQUIRE_FALSE(last.has_value());
    CHECK(depth.tracks.size() == 1);
    CHECK(TrackOf(depth, "Translation").keys.size() == 1);
}

TEST_CASE("A keyframe of a property the depth does not animate cannot be reached") {
    Document::AuthoredDepth depth = Depth();
    CHECK_FALSE(Document::RemoveKeyAt(depth, "Multiply colour", 0).has_value());
    CHECK_FALSE(Document::MoveKeyTo(depth, "Multiply colour", 0, 1).has_value());
    CHECK_FALSE(Document::SetKeyValueAt(depth, "Multiply colour", 0, "1").has_value());
    CHECK_FALSE(Document::SetKeyEaseAt(depth, "Multiply colour", 0, Document::Ease::Linear, {})
                    .has_value());
}
