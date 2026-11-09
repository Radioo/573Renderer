#include "document/filter_fields.h"

#include "document/field_values.h"
#include "document/outline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr std::string_view kPrefix = "Filter ";
constexpr std::string_view kHsv = "HSV";
constexpr std::size_t kRowWidth = 5;
constexpr std::array<std::string_view, 4> kRows{"red", "green", "blue", "alpha"};
constexpr std::array<uint8_t, 4> kMatrixHead{6, 0, 0, 0};
constexpr std::array<uint8_t, 4> kHsvHead{6, 1, 0x64, 0};
constexpr int32_t kOne = 65536;

struct FieldName {
    std::size_t filter = 0;
    std::string_view part;
};

std::optional<FieldName> Parse(std::string_view name) {
    if (!name.starts_with(kPrefix)) return std::nullopt;
    name.remove_prefix(kPrefix.size());
    std::size_t number = 0;
    const auto parsed = std::from_chars(name.data(), name.data() + name.size(), number);
    if (parsed.ec != std::errc{} || number == 0) return std::nullopt;
    std::string_view rest(parsed.ptr, name.data() + name.size() - parsed.ptr);
    if (rest.empty()) return FieldName{.filter = number - 1, .part = {}};
    if (!rest.starts_with(' ')) return std::nullopt;
    rest.remove_prefix(1);
    return FieldName{.filter = number - 1, .part = rest};
}

std::string Label(std::size_t filter, std::string_view part) {
    std::string label = std::string(kPrefix) + std::to_string(filter + 1);
    if (!part.empty()) label += " " + std::string(part);
    return label;
}

std::string Kind(const AfpAnimation::Filter& filter) {
    if (const auto* matrix = std::get_if<AfpAnimation::ColourMatrixFilter>(&filter))
        return matrix->hsv ? "colour matrix with HSV" : "colour matrix";
    if (const auto* lookup = std::get_if<AfpAnimation::LookupFilter>(&filter))
        return "lookup, " + std::to_string(lookup->table.size()) + " table bytes";
    const auto& unknown = std::get<AfpAnimation::UnknownFilter>(filter);
    return "unknown, " + std::to_string(unknown.bytes.size()) + " bytes";
}

std::string RowText(const AfpAnimation::ColourMatrixFilter& matrix, std::size_t row) {
    std::vector<std::string> parts;
    parts.reserve(kRowWidth);
    for (std::size_t column = 0; column < kRowWidth; column++)
        parts.push_back(std::to_string(matrix.matrix.at((row * kRowWidth) + column)));
    return Join(parts);
}

std::optional<std::size_t> RowOf(std::string_view part) {
    const auto found = std::ranges::find(kRows, part);
    if (found == kRows.end()) return std::nullopt;
    return static_cast<std::size_t>(found - kRows.begin());
}

bool Fits(int64_t value, int64_t lowest, int64_t highest) {
    return value >= lowest && value <= highest;
}

Support::Expected<void, std::string> SetRow(AfpAnimation::ColourMatrixFilter& matrix,
                                            std::size_t row, std::string_view value) {
    auto numbers = Numbers(value, kRowWidth);
    if (!numbers) return Support::Unexpected(numbers.error());
    for (std::size_t column = 0; column < kRowWidth; column++) {
        const int64_t number = (*numbers)[column];
        if (!Fits(number, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()))
            return Support::Unexpected(std::to_string(number) + " does not fit a matrix entry");
        matrix.matrix.at((row * kRowWidth) + column) = static_cast<int32_t>(number);
    }
    return {};
}

Support::Expected<void, std::string> SetHsv(AfpAnimation::ColourMatrixFilter& matrix,
                                            std::string_view value) {
    if (!matrix.hsv)
        return Support::Unexpected(std::string("this colour matrix carries no HSV to change"));
    auto numbers = Numbers(value, 3);
    if (!numbers) return Support::Unexpected(numbers.error());
    const std::vector<int64_t>& hsv = *numbers;
    if (!Fits(hsv[0], std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max()) ||
        !Fits(hsv[1], std::numeric_limits<int8_t>::min(), std::numeric_limits<int8_t>::max()) ||
        !Fits(hsv[2], std::numeric_limits<int8_t>::min(), std::numeric_limits<int8_t>::max())) {
        return Support::Unexpected(std::string("an HSV value does not fit the filter"));
    }
    matrix.hsv = AfpAnimation::Hsv{.hue = static_cast<int16_t>(hsv[0]),
                                   .saturation = static_cast<int8_t>(hsv[1]),
                                   .value = static_cast<int8_t>(hsv[2])};
    return {};
}

}

std::vector<Field> FilterFields(const std::vector<AfpAnimation::Filter>& filters) {
    std::vector<Field> fields;
    for (std::size_t i = 0; i < filters.size(); i++) {
        fields.push_back(Field{.name = Label(i, {}), .value = Kind(filters[i])});
        const auto* matrix = std::get_if<AfpAnimation::ColourMatrixFilter>(&filters[i]);
        if (matrix == nullptr) continue;
        for (std::size_t row = 0; row < kRows.size(); row++) {
            fields.push_back(
                Field{.name = Label(i, kRows.at(row)), .value = RowText(*matrix, row)});
        }
        if (matrix->hsv) {
            fields.push_back(Field{.name = Label(i, kHsv),
                                   .value = Join({std::to_string(matrix->hsv->hue),
                                                  std::to_string(matrix->hsv->saturation),
                                                  std::to_string(matrix->hsv->value)})});
        }
    }
    return fields;
}

bool FilterFieldIsEditable(std::string_view name) {
    const std::optional<FieldName> parsed = Parse(name);
    return parsed && (RowOf(parsed->part) || parsed->part == kHsv);
}

Support::Expected<void, std::string> SetFilterField(std::vector<AfpAnimation::Filter>& filters,
                                                    std::string_view name, std::string_view value) {
    const std::optional<FieldName> parsed = Parse(name);
    if (!parsed || !FilterFieldIsEditable(name))
        return Support::Unexpected(std::string(name) + " is not an editable filter field");
    if (parsed->filter >= filters.size())
        return Support::Unexpected("there is no filter " + std::to_string(parsed->filter + 1));
    auto* matrix = std::get_if<AfpAnimation::ColourMatrixFilter>(&filters[parsed->filter]);
    if (matrix == nullptr) {
        return Support::Unexpected("filter " + std::to_string(parsed->filter + 1) +
                                   " is not a colour matrix");
    }
    std::vector<AfpAnimation::Filter> edited = filters;
    auto* target = std::get_if<AfpAnimation::ColourMatrixFilter>(&edited[parsed->filter]);
    const std::optional<std::size_t> row = RowOf(parsed->part);
    auto set = row ? SetRow(*target, *row, value) : SetHsv(*target, value);
    if (!set) return Support::Unexpected(set.error());
    filters = std::move(edited);
    return {};
}

std::optional<std::size_t> FilterNumberOf(std::string_view name) {
    const std::optional<FieldName> parsed = Parse(name);
    if (!parsed) return std::nullopt;
    return parsed->filter + 1;
}

void AddFilter(std::vector<AfpAnimation::Filter>& filters, NewFilter kind) {
    AfpAnimation::ColourMatrixFilter matrix;
    matrix.head = kind == NewFilter::Hsv ? kHsvHead : kMatrixHead;
    for (std::size_t i = 0; i < kRows.size(); i++)
        matrix.matrix.at((i * kRowWidth) + i) = kOne;
    if (kind == NewFilter::Hsv) matrix.hsv = AfpAnimation::Hsv{};
    filters.emplace_back(matrix);
}

Support::Expected<void, std::string> RemoveFilter(std::vector<AfpAnimation::Filter>& filters,
                                                  std::string_view name) {
    const std::optional<std::size_t> number = FilterNumberOf(name);
    if (!number) return Support::Unexpected(std::string(name) + " names no filter");
    if (*number > filters.size())
        return Support::Unexpected("there is no filter " + std::to_string(*number));
    filters.erase(filters.begin() + static_cast<std::ptrdiff_t>(*number - 1));
    return {};
}

}
