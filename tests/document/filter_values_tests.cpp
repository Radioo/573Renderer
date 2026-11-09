#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include "document/filter_values.h"
#include "formats/afp_animation.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

std::vector<AfpAnimation::Filter> Filters() {
    AfpAnimation::ColourMatrixFilter plain;
    plain.head = {6, 0, 0, 0};
    for (std::size_t i = 0; i < plain.matrix.size(); i++)
        plain.matrix.at(i) = static_cast<int32_t>(i * 65536) - 70000;
    AfpAnimation::ColourMatrixFilter shifted = plain;
    shifted.head = {6, 1, 0x64, 0};
    shifted.hsv = AfpAnimation::Hsv{.hue = -180, .saturation = -100, .value = 100};
    const AfpAnimation::LookupFilter lookup{
        .head = {0x67, 0, 0x3E, 2, 0, 0}, .unread_bytes = {1, 0, 1, 0}, .table = {0, 255, 7}};
    const AfpAnimation::UnknownFilter unknown{.bytes = {1, 0, 0, 0, 9, 9, 9, 9}};
    return {plain, shifted, lookup, unknown};
}

}

TEST_CASE("A filter list turns into numbers and back unchanged") {
    const std::vector<AfpAnimation::Filter> filters = Filters();
    const std::vector<int64_t> numbers = Document::FilterNumbers(filters);
    CHECK(numbers.front() == 4);
    const auto back = Document::FiltersFrom(numbers);
    const std::string error = back.has_value() ? std::string() : back.error();
    INFO(error);
    REQUIRE(back.has_value());
    CHECK(*back == filters);

    const auto empty = Document::FiltersFrom(Document::FilterNumbers({}));
    REQUIRE(empty.has_value());
    CHECK(empty->empty());
}

TEST_CASE("Two lists of the same kinds of filter take the same count of numbers") {
    std::vector<AfpAnimation::Filter> other = Filters();
    std::get<AfpAnimation::ColourMatrixFilter>(other[0]).matrix[3] = 12345;
    CHECK(Document::FilterNumbers(other).size() == Document::FilterNumbers(Filters()).size());
}

TEST_CASE("Numbers that are not a filter list are refused") {
    std::vector<int64_t> numbers = Document::FilterNumbers(Filters());
    CHECK_FALSE(Document::FiltersFrom({}).has_value());

    std::vector<int64_t> short_list(numbers.begin(), numbers.end() - 1);
    CHECK_FALSE(Document::FiltersFrom(short_list).has_value());

    std::vector<int64_t> longer = numbers;
    longer.push_back(0);
    CHECK_FALSE(Document::FiltersFrom(longer).has_value());

    std::vector<int64_t> wrong_kind = numbers;
    wrong_kind[1] = 7;
    CHECK_FALSE(Document::FiltersFrom(wrong_kind).has_value());

    std::vector<int64_t> too_big = numbers;
    too_big[2] = 256;
    CHECK_FALSE(Document::FiltersFrom(too_big).has_value());
}
