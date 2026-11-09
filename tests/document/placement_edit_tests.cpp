#include <catch2/catch_test_macros.hpp>

#include "document/outline.h"
#include "document/placement_edit.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kCreate = 0x2;

AfpAnimation::Tag Place(uint16_t depth) {
    AfpAnimation::Placement placement;
    placement.flags = kCreate;
    placement.depth = depth;
    return AfpAnimation::Tag{placement};
}

AfpAnimation::Container ClipOf(const std::vector<std::vector<AfpAnimation::Tag>>& per_frame) {
    AfpAnimation::Container clip;
    for (const std::vector<AfpAnimation::Tag>& tags : per_frame) {
        clip.frames.push_back(
            AfpAnimation::Frame{.first_tag = static_cast<uint32_t>(clip.tags.size()),
                                .tag_count = static_cast<uint32_t>(tags.size())});
        for (const AfpAnimation::Tag& tag : tags) {
            clip.tags.push_back(tag);
        }
    }
    return clip;
}

std::string ValueOf(const std::vector<Document::Field>& fields, const std::string& name) {
    const auto found = std::ranges::find(fields, name, &Document::Field::name);
    return found == fields.end() ? std::string("<missing>") : found->value;
}

}

TEST_CASE("The live placement at a depth is the last one before the frame") {
    const AfpAnimation::Container clip = ClipOf({
        {Place(3)},
        {Place(3)},
        {AfpAnimation::Tag{AfpAnimation::Remove{.unread_word = 0, .depth = 3}}},
        {},
    });
    CHECK(Document::LivePlacementTag(clip, 3, 0) == 0);
    CHECK(Document::LivePlacementTag(clip, 3, 1) == 1);
    CHECK_FALSE(Document::LivePlacementTag(clip, 3, 2).has_value());
    CHECK_FALSE(Document::LivePlacementTag(clip, 4, 1).has_value());
}

TEST_CASE("Placement fields show what the placement carries and nothing else") {
    AfpAnimation::Animation animation;
    animation.strings = {"", "logo"};
    AfpAnimation::Placement placement;
    placement.depth = 7;
    placement.end_frame = 42;
    placement.flags = kCreate;
    placement.translation = {{100, -200}};
    placement.name = 1;
    placement.hsv = AfpAnimation::Hsv{.hue = 90, .saturation = -5, .value = 3};
    placement.filters = std::vector<AfpAnimation::Filter>{
        AfpAnimation::Filter{AfpAnimation::UnknownFilter{.bytes = {1, 2, 3}}}};

    const std::vector<Document::Field> fields = Document::PlacementFields(animation, placement);
    CHECK(ValueOf(fields, "Depth") == "7");
    CHECK(ValueOf(fields, "End frame") == "42");
    CHECK(ValueOf(fields, "Updates the depth") == "no");
    CHECK(ValueOf(fields, "Translation") == "100, -200");
    CHECK(ValueOf(fields, "Name") == "logo");
    CHECK(ValueOf(fields, "HSV") == "90, -5, 3");
    CHECK(ValueOf(fields, "Scale").empty());
    CHECK(ValueOf(fields, "Filter 1") == "unknown, 3 bytes");
    CHECK(ValueOf(fields, "Unknown data") == "<missing>");
    CHECK(Document::PlacementFieldIsEditable("Translation"));
    CHECK_FALSE(Document::PlacementFieldIsEditable("Unknown data"));
    CHECK_FALSE(Document::PlacementFieldIsEditable("Depth"));
}

TEST_CASE("Setting a field adds it and clearing it takes it away") {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    AfpAnimation::Placement placement;

    REQUIRE(Document::SetPlacementField(animation, placement, "Scale", "1024, 2048").has_value());
    REQUIRE(placement.scale.has_value());
    CHECK((*placement.scale)[0] == 1024);
    CHECK((*placement.scale)[1] == 2048);

    REQUIRE(Document::SetPlacementField(animation, placement, "Scale", "").has_value());
    CHECK_FALSE(placement.scale.has_value());
}

TEST_CASE("Setting a name interns it in the string table once") {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    AfpAnimation::Placement first;
    AfpAnimation::Placement second;
    REQUIRE(Document::SetPlacementField(animation, first, "Name", "logo").has_value());
    REQUIRE(Document::SetPlacementField(animation, second, "Name", "logo").has_value());
    REQUIRE(animation.strings.size() == 2);
    CHECK(animation.strings[1] == "logo");
    CHECK(first.name == second.name);
}

TEST_CASE("A field refuses a value it cannot hold") {
    AfpAnimation::Animation animation;
    AfpAnimation::Placement placement;
    CHECK_FALSE(Document::SetPlacementField(animation, placement, "Blend", "300").has_value());
    CHECK_FALSE(Document::SetPlacementField(animation, placement, "Blend", "-1").has_value());
    CHECK_FALSE(Document::SetPlacementField(animation, placement, "Scale", "1024").has_value());
    CHECK_FALSE(Document::SetPlacementField(animation, placement, "Scale", "a, b").has_value());
    CHECK_FALSE(Document::SetPlacementField(animation, placement, "Filters", "1").has_value());
    CHECK_FALSE(placement.blend.has_value());
    CHECK_FALSE(placement.scale.has_value());
}

TEST_CASE("Every editable field round trips through its text") {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    AfpAnimation::Placement placement;
    const std::vector<Document::Field> empty = Document::PlacementFields(animation, placement);
    for (const Document::Field& field : empty) {
        if (!Document::PlacementFieldIsEditable(field.name)) continue;
        CHECK(field.value.empty());
    }
    REQUIRE(
        Document::SetPlacementField(animation, placement, "3D matrix", "1, 2, 3, 4, 5, 6, 7, 8, 9")
            .has_value());
    const std::vector<Document::Field> filled = Document::PlacementFields(animation, placement);
    CHECK(ValueOf(filled, "3D matrix") == "1, 2, 3, 4, 5, 6, 7, 8, 9");
}
