#include <catch2/catch_test_macros.hpp>

#include "document/authored.h"
#include "document/filter_fields.h"
#include "document/filter_values.h"
#include "document/inspector.h"
#include "document/keyframe_edit.h"
#include "document/keyframes.h"
#include "document/outline.h"
#include "document/placement_edit.h"
#include "formats/afp_animation.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

std::vector<AfpAnimation::Filter> Filters() {
    AfpAnimation::ColourMatrixFilter matrix;
    matrix.head = {6, 1, 0x64, 0};
    for (std::size_t i = 0; i < matrix.matrix.size(); i++)
        matrix.matrix.at(i) = static_cast<int32_t>(i);
    matrix.hsv = AfpAnimation::Hsv{.hue = 10, .saturation = -5, .value = 3};
    const AfpAnimation::LookupFilter lookup{
        .head = {0x67, 0, 2, 2, 0, 0}, .unread_bytes = {1, 0, 1, 0}, .table = {1, 2, 3}};
    return {matrix, lookup};
}

std::optional<std::string> ValueOf(const std::vector<Document::Field>& fields,
                                   const std::string& name) {
    const auto found = std::ranges::find(fields, name, &Document::Field::name);
    if (found == fields.end()) return std::nullopt;
    return found->value;
}

const AfpAnimation::ColourMatrixFilter& MatrixOf(const std::vector<AfpAnimation::Filter>& filters) {
    return std::get<AfpAnimation::ColourMatrixFilter>(filters.front());
}

}

TEST_CASE("A filter list reads as one row per filter and per colour matrix row") {
    const std::vector<Document::Field> fields = Document::FilterFields(Filters());
    CHECK(ValueOf(fields, "Filter 1") == "colour matrix with HSV");
    CHECK(ValueOf(fields, "Filter 1 red") == "0, 1, 2, 3, 4");
    CHECK(ValueOf(fields, "Filter 1 alpha") == "15, 16, 17, 18, 19");
    CHECK(ValueOf(fields, "Filter 1 HSV") == "10, -5, 3");
    CHECK(ValueOf(fields, "Filter 2") == "lookup, 3 table bytes");
    CHECK_FALSE(ValueOf(fields, "Filter 2 red").has_value());
    CHECK(Document::FilterFieldIsEditable("Filter 1 green"));
    CHECK(Document::FilterFieldIsEditable("Filter 12 HSV"));
    CHECK_FALSE(Document::FilterFieldIsEditable("Filter 1"));
    CHECK_FALSE(Document::FilterFieldIsEditable("Filter 0 red"));
    CHECK_FALSE(Document::FilterFieldIsEditable("Filter 1 purple"));
    CHECK_FALSE(Document::FilterFieldIsEditable("Scale"));
}

TEST_CASE("A colour matrix row and its HSV are changed from text") {
    std::vector<AfpAnimation::Filter> filters = Filters();
    REQUIRE(Document::SetFilterField(filters, "Filter 1 blue", "65536, 0, 0, 0, -20").has_value());
    CHECK(MatrixOf(filters).matrix[10] == 65536);
    CHECK(MatrixOf(filters).matrix[14] == -20);
    REQUIRE(Document::SetFilterField(filters, "Filter 1 HSV", "-180, 100, -100").has_value());
    CHECK(MatrixOf(filters).hsv ==
          AfpAnimation::Hsv{.hue = -180, .saturation = 100, .value = -100});
}

TEST_CASE("A filter edit that does not fit changes nothing") {
    std::vector<AfpAnimation::Filter> filters = Filters();
    const std::vector<AfpAnimation::Filter> before = filters;
    CHECK_FALSE(Document::SetFilterField(filters, "Filter 1 red", "1, 2, 3").has_value());
    CHECK_FALSE(
        Document::SetFilterField(filters, "Filter 1 red", "1, 2, 3, 4, 9999999999").has_value());
    CHECK_FALSE(Document::SetFilterField(filters, "Filter 1 HSV", "0, 200, 0").has_value());
    CHECK_FALSE(Document::SetFilterField(filters, "Filter 2 red", "1, 2, 3, 4, 5").has_value());
    CHECK_FALSE(Document::SetFilterField(filters, "Filter 3 red", "1, 2, 3, 4, 5").has_value());
    CHECK_FALSE(Document::SetFilterField(filters, "Filter 1", "colour matrix").has_value());
    CHECK(filters == before);

    std::vector<AfpAnimation::Filter> plain = Filters();
    std::get<AfpAnimation::ColourMatrixFilter>(plain.front()).hsv.reset();
    CHECK_FALSE(Document::SetFilterField(plain, "Filter 1 HSV", "1, 1, 1").has_value());
}

TEST_CASE("A placement's filters show and change through its fields") {
    AfpAnimation::Animation animation;
    animation.strings = {""};
    AfpAnimation::Placement placement;
    placement.filters = Filters();
    const std::vector<Document::Field> fields = Document::PlacementFields(animation, placement);
    CHECK(ValueOf(fields, "Filter 1 green") == "5, 6, 7, 8, 9");
    CHECK_FALSE(ValueOf(fields, "Unknown data").has_value());
    CHECK(Document::PlacementFieldIsEditable("Filter 1 green"));
    REQUIRE(Document::SetPlacementField(animation, placement, "Filter 1 green", "9, 9, 9, 9, 9")
                .has_value());
    REQUIRE(placement.filters.has_value());
    if (!placement.filters) return;
    CHECK(MatrixOf(*placement.filters).matrix[5] == 9);

    AfpAnimation::Placement bare;
    CHECK_FALSE(Document::SetPlacementField(animation, bare, "Filter 1 green", "1, 1, 1, 1, 1")
                    .has_value());
}

TEST_CASE("A filter keyframe shows its filters and takes an edit to one row") {
    Document::AuthoredDepth depth{
        .animation = "afp/a",
        .depth = 1,
        .first_frame = 0,
        .last_frame = 4,
        .tracks = {Document::Track{
            .property = "Filters",
            .keys = {Document::Keyframe{.frame = 0,
                                        .value = Document::FilterNumbers(Filters()),
                                        .ease = Document::Ease::Hold,
                                        .bezier = {}}}}},
        .script = std::nullopt,
        .clip = {}};
    const AfpAnimation::Animation animation;
    const std::vector<Document::InspectedRow> rows =
        Document::InspectFrame(animation, Document::Selection{.depth = std::nullopt,
                                                              .frame = 0,
                                                              .owned = &depth,
                                                              .key_property = "Filters",
                                                              .key_frame = 0,
                                                              .clip = {}});
    const auto red = std::ranges::find_if(
        rows, [](const Document::InspectedRow& row) { return row.field.name == "Filter 1 red"; });
    REQUIRE(red != rows.end());
    CHECK(red->edits == Document::EditTarget::KeyFilter);
    CHECK(std::ranges::none_of(rows, [](const Document::InspectedRow& row) {
        return row.field.name == "Keyframe value";
    }));

    REQUIRE(Document::SetKeyFilterFieldAt(depth, 0, "Filter 1 red", "7, 7, 7, 7, 7").has_value());
    const auto decoded = Document::FiltersFrom(depth.tracks[0].keys[0].value);
    REQUIRE(decoded.has_value());
    if (!decoded) return;
    CHECK(MatrixOf(*decoded).matrix[0] == 7);
    CHECK_FALSE(
        Document::SetKeyFilterFieldAt(depth, 3, "Filter 1 red", "7, 7, 7, 7, 7").has_value());
}

TEST_CASE("A new filter is a colour matrix or an HSV filter that changes nothing") {
    std::vector<AfpAnimation::Filter> filters;
    Document::AddFilter(filters, Document::NewFilter::ColourMatrix);
    Document::AddFilter(filters, Document::NewFilter::Hsv);
    REQUIRE(filters.size() == 2);
    const auto& plain = std::get<AfpAnimation::ColourMatrixFilter>(filters[0]);
    const auto& hsv = std::get<AfpAnimation::ColourMatrixFilter>(filters[1]);
    CHECK(plain.head == std::array<uint8_t, 4>{6, 0, 0, 0});
    CHECK_FALSE(plain.hsv.has_value());
    CHECK(hsv.head == std::array<uint8_t, 4>{6, 1, 0x64, 0});
    CHECK(hsv.hsv == AfpAnimation::Hsv{.hue = 0, .saturation = 0, .value = 0});
    for (const auto* matrix : {&plain, &hsv}) {
        for (std::size_t i = 0; i < matrix->matrix.size(); i++)
            CHECK(matrix->matrix.at(i) == (i % 6 == 0 && i < 20 ? 65536 : 0));
    }
    const auto decoded = Document::FiltersFrom(Document::FilterNumbers(filters));
    REQUIRE(decoded.has_value());
    if (!decoded) return;
    CHECK(*decoded == filters);
}

TEST_CASE("A filter is removed by the name of any of its rows") {
    std::vector<AfpAnimation::Filter> filters = Filters();
    CHECK(Document::FilterNumberOf("Filter 12 HSV") == std::size_t{12});
    CHECK_FALSE(Document::FilterNumberOf("Filter 0").has_value());
    CHECK_FALSE(Document::FilterNumberOf("Filters").has_value());
    REQUIRE(Document::RemoveFilter(filters, "Filter 1 red").has_value());
    REQUIRE(filters.size() == 1);
    CHECK(std::holds_alternative<AfpAnimation::LookupFilter>(filters[0]));
    CHECK_FALSE(Document::RemoveFilter(filters, "Filter 2").has_value());
    CHECK_FALSE(Document::RemoveFilter(filters, "Blend").has_value());
    CHECK(filters.size() == 1);
}

TEST_CASE("A filter keyframe gains and loses filters down to none") {
    Document::AuthoredDepth depth{
        .animation = "afp/a",
        .depth = 1,
        .first_frame = 0,
        .last_frame = 4,
        .tracks = {Document::Track{
            .property = "Filters",
            .keys = {Document::Keyframe{.frame = 0,
                                        .value = Document::FilterNumbers(Filters()),
                                        .ease = Document::Ease::Hold,
                                        .bezier = {}}}}},
        .script = std::nullopt,
        .clip = {}};
    const auto Count = [&depth] {
        const auto decoded = Document::FiltersFrom(depth.tracks[0].keys[0].value);
        return decoded ? decoded->size() : std::size_t{0};
    };
    REQUIRE(Document::AddKeyFilterAt(depth, 0, Document::NewFilter::ColourMatrix).has_value());
    CHECK(Count() == 3);
    CHECK_FALSE(Document::AddKeyFilterAt(depth, 2, Document::NewFilter::Hsv).has_value());
    REQUIRE(Document::RemoveKeyFilterAt(depth, 0, "Filter 1").has_value());
    REQUIRE(Document::RemoveKeyFilterAt(depth, 0, "Filter 1").has_value());
    CHECK(Count() == 1);
    REQUIRE(Document::RemoveKeyFilterAt(depth, 0, "Filter 1").has_value());
    CHECK(Count() == 0);
    CHECK(depth.tracks[0].keys[0].value == std::vector<int64_t>{0});
    CHECK_FALSE(Document::RemoveKeyFilterAt(depth, 0, "Filter 1").has_value());
}
