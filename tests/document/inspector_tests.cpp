#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/inspector.h"
#include "document/keyframes.h"
#include "formats/afp_animation.h"

#include "sample_package.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

AfpAnimation::Animation Scene() {
    AfpAnimation::Animation animation = SamplePackage::SampleAnimation();
    AfpAnimation::Placement create;
    create.depth = 1;
    create.end_frame = 3;
    create.character = uint16_t{7};
    create.translation = std::array<int32_t, 2>{10, 20};
    animation.root.tags.push_back(AfpAnimation::Tag{create});
    animation.root.frames[0].tag_count = 1;
    return animation;
}

Document::AuthoredDepth Owned() {
    Document::Track track{.property = "Translation", .keys = {}};
    track.keys.push_back(Document::Keyframe{
        .frame = 0, .value = {10, 20}, .ease = Document::Ease::Linear, .bezier = {}});
    track.keys.push_back(Document::Keyframe{
        .frame = 2, .value = {90, 20}, .ease = Document::Ease::Linear, .bezier = {}});
    return Document::AuthoredDepth{.animation = "afp/scene",
                                   .depth = 1,
                                   .first_frame = 0,
                                   .last_frame = 2,
                                   .tracks = {track},
                                   .script = std::nullopt};
}

const Document::InspectedRow* RowNamed(const std::vector<Document::InspectedRow>& rows,
                                       const std::string& name) {
    const auto found = std::ranges::find_if(
        rows, [&name](const Document::InspectedRow& row) { return row.field.name == name; });
    return found == rows.end() ? nullptr : &*found;
}

}

TEST_CASE("A selected depth is inspected as its placement fields") {
    const AfpAnimation::Animation animation = Scene();
    const std::vector<Document::InspectedRow> rows =
        Document::InspectFrame(animation, Document::Selection{.depth = uint16_t{1},
                                                              .frame = 0,
                                                              .owned = nullptr,
                                                              .key_property = {},
                                                              .key_frame = std::nullopt});

    const Document::InspectedRow* translation = RowNamed(rows, "Translation");
    REQUIRE(translation != nullptr);
    CHECK(translation->field.value == "10, 20");
    CHECK(translation->edits == Document::EditTarget::Placement);
    CHECK(RowNamed(rows, "Depth") != nullptr);
}

TEST_CASE("A depth that holds nothing on a frame says so and edits nothing") {
    const AfpAnimation::Animation animation = Scene();
    const std::vector<Document::InspectedRow> rows =
        Document::InspectFrame(animation, Document::Selection{.depth = uint16_t{9},
                                                              .frame = 0,
                                                              .owned = nullptr,
                                                              .key_property = {},
                                                              .key_frame = std::nullopt});

    const Document::InspectedRow* depth = RowNamed(rows, "Depth");
    REQUIRE(depth != nullptr);
    CHECK(depth->field.value.find("holds nothing") != std::string::npos);
    CHECK(std::ranges::none_of(rows, [](const Document::InspectedRow& row) {
        return row.edits != Document::EditTarget::None;
    }));
}

TEST_CASE("Nothing selected inspects nothing") {
    const AfpAnimation::Animation animation = Scene();
    CHECK(Document::InspectFrame(animation, Document::Selection{}).empty());
}

TEST_CASE("An owned depth keeps its own row alongside the placement fields") {
    const AfpAnimation::Animation animation = Scene();
    const Document::AuthoredDepth owned = Owned();
    const std::vector<Document::InspectedRow> rows =
        Document::InspectFrame(animation, Document::Selection{.depth = uint16_t{1},
                                                              .frame = 0,
                                                              .owned = &owned,
                                                              .key_property = {},
                                                              .key_frame = std::nullopt});

    const Document::InspectedRow* said = RowNamed(rows, "Owned by the project");
    REQUIRE(said != nullptr);
    CHECK(said->field.value.find("frames 0 to 2") != std::string::npos);
    REQUIRE(RowNamed(rows, "Translation") != nullptr);
}

TEST_CASE("An owned depth's placement fields are not editable") {
    const AfpAnimation::Animation animation = Scene();
    const Document::AuthoredDepth owned = Owned();
    const std::vector<Document::InspectedRow> rows =
        Document::InspectFrame(animation, Document::Selection{.depth = uint16_t{1},
                                                              .frame = 0,
                                                              .owned = &owned,
                                                              .key_property = {},
                                                              .key_frame = std::nullopt});

    const Document::InspectedRow* translation = RowNamed(rows, "Translation");
    REQUIRE(translation != nullptr);
    CHECK(translation->edits == Document::EditTarget::None);
}

TEST_CASE("A selected keyframe is the one editable row of an owned depth") {
    const AfpAnimation::Animation animation = Scene();
    const Document::AuthoredDepth owned = Owned();
    const std::vector<Document::InspectedRow> rows =
        Document::InspectFrame(animation, Document::Selection{.depth = uint16_t{1},
                                                              .frame = 0,
                                                              .owned = &owned,
                                                              .key_property = "Translation",
                                                              .key_frame = 2});

    const Document::InspectedRow* which = RowNamed(rows, "Keyframe");
    REQUIRE(which != nullptr);
    CHECK(which->field.value == "Translation on frame 2");
    const Document::InspectedRow* value = RowNamed(rows, "Keyframe value");
    REQUIRE(value != nullptr);
    CHECK(value->field.value == "90, 20");
    CHECK(value->edits == Document::EditTarget::KeyValue);
    const Document::InspectedRow* ease = RowNamed(rows, "Keyframe leaves as");
    REQUIRE(ease != nullptr);
    CHECK(ease->field.value == "linear");

    const auto editable = std::ranges::count_if(rows, [](const Document::InspectedRow& row) {
        return row.edits != Document::EditTarget::None;
    });
    CHECK(editable == 1);
}

TEST_CASE("A keyframe that is no longer there is not inspected") {
    const AfpAnimation::Animation animation = Scene();
    const Document::AuthoredDepth owned = Owned();
    const std::vector<Document::InspectedRow> rows =
        Document::InspectFrame(animation, Document::Selection{.depth = uint16_t{1},
                                                              .frame = 0,
                                                              .owned = &owned,
                                                              .key_property = "Translation",
                                                              .key_frame = 1});
    CHECK(RowNamed(rows, "Keyframe value") == nullptr);
    CHECK(RowNamed(rows, "Owned by the project") != nullptr);
}

TEST_CASE("A camera on the frame is inspected under whatever the depth holds") {
    AfpAnimation::Animation animation = Scene();
    AfpAnimation::Camera camera;
    camera.focal_length = 1234;
    animation.root.tags.push_back(AfpAnimation::Tag{camera});
    animation.root.frames[0].tag_count = 2;

    const std::vector<Document::InspectedRow> rows =
        Document::InspectFrame(animation, Document::Selection{.depth = uint16_t{1},
                                                              .frame = 0,
                                                              .owned = nullptr,
                                                              .key_property = {},
                                                              .key_frame = std::nullopt});
    const Document::InspectedRow* focal = RowNamed(rows, "Focal length");
    REQUIRE(focal != nullptr);
    CHECK(focal->edits == Document::EditTarget::Camera);
}

TEST_CASE("A camera is inspected even when no depth is selected") {
    AfpAnimation::Animation animation = Scene();
    AfpAnimation::Camera camera;
    camera.focal_length = 1234;
    animation.root.tags.push_back(AfpAnimation::Tag{camera});
    animation.root.frames[0].tag_count = 2;

    const std::vector<Document::InspectedRow> rows =
        Document::InspectFrame(animation, Document::Selection{.depth = std::nullopt,
                                                              .frame = 0,
                                                              .owned = nullptr,
                                                              .key_property = {},
                                                              .key_frame = std::nullopt});
    CHECK(RowNamed(rows, "Focal length") != nullptr);
}
