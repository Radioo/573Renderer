#include "document/filter_values.h"

#include "document/number_reader.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr int64_t kColourMatrix = 0;
constexpr int64_t kLookup = 1;
constexpr int64_t kUnknown = 2;

void Append(std::vector<int64_t>& out, std::span<const uint8_t> bytes) {
    out.push_back(static_cast<int64_t>(bytes.size()));
    for (const uint8_t byte : bytes)
        out.push_back(byte);
}

std::optional<AfpAnimation::Filter> ColourMatrixFrom(NumberReader& in) {
    AfpAnimation::ColourMatrixFilter filter;
    if (!in.Fill(filter.head) || !in.Fill(filter.matrix)) return std::nullopt;
    const std::optional<uint8_t> has_hsv = in.Next<uint8_t>();
    const std::optional<int16_t> hue = in.Next<int16_t>();
    const std::optional<int8_t> saturation = in.Next<int8_t>();
    const std::optional<int8_t> value = in.Next<int8_t>();
    if (!has_hsv || *has_hsv > 1 || !hue || !saturation || !value) return std::nullopt;
    if (*has_hsv == 1)
        filter.hsv = AfpAnimation::Hsv{.hue = *hue, .saturation = *saturation, .value = *value};
    return filter;
}

std::optional<AfpAnimation::Filter> LookupFrom(NumberReader& in) {
    AfpAnimation::LookupFilter filter;
    if (!in.Fill(filter.head) || !in.Fill(filter.unread_bytes)) return std::nullopt;
    std::optional<std::vector<uint8_t>> table = in.Bytes();
    if (!table) return std::nullopt;
    filter.table = std::move(*table);
    return filter;
}

std::optional<AfpAnimation::Filter> UnknownFrom(NumberReader& in) {
    std::optional<std::vector<uint8_t>> bytes = in.Bytes();
    if (!bytes) return std::nullopt;
    return AfpAnimation::UnknownFilter{.bytes = std::move(*bytes)};
}

}

std::vector<int64_t> FilterNumbers(const std::vector<AfpAnimation::Filter>& filters) {
    std::vector<int64_t> out{static_cast<int64_t>(filters.size())};
    for (const AfpAnimation::Filter& filter : filters) {
        if (const auto* matrix = std::get_if<AfpAnimation::ColourMatrixFilter>(&filter)) {
            out.push_back(kColourMatrix);
            out.insert(out.end(), matrix->head.begin(), matrix->head.end());
            out.insert(out.end(), matrix->matrix.begin(), matrix->matrix.end());
            const AfpAnimation::Hsv hsv = matrix->hsv.value_or(AfpAnimation::Hsv{});
            out.insert(out.end(), {matrix->hsv ? 1 : 0, hsv.hue, hsv.saturation, hsv.value});
        } else if (const auto* lookup = std::get_if<AfpAnimation::LookupFilter>(&filter)) {
            out.push_back(kLookup);
            out.insert(out.end(), lookup->head.begin(), lookup->head.end());
            out.insert(out.end(), lookup->unread_bytes.begin(), lookup->unread_bytes.end());
            Append(out, lookup->table);
        } else if (const auto* unknown = std::get_if<AfpAnimation::UnknownFilter>(&filter)) {
            out.push_back(kUnknown);
            Append(out, unknown->bytes);
        }
    }
    return out;
}

Support::Expected<std::vector<AfpAnimation::Filter>, std::string>
FiltersFrom(std::span<const int64_t> numbers) {
    NumberReader in(numbers);
    const std::optional<uint16_t> count = in.Next<uint16_t>();
    if (!count) return Support::Unexpected(std::string("a filter list starts with its count"));
    std::vector<AfpAnimation::Filter> filters;
    for (uint16_t i = 0; i < *count; i++) {
        const std::optional<uint8_t> kind = in.Next<uint8_t>();
        std::optional<AfpAnimation::Filter> filter;
        if (kind == kColourMatrix) {
            filter = ColourMatrixFrom(in);
        } else if (kind == kLookup) {
            filter = LookupFrom(in);
        } else if (kind == kUnknown) {
            filter = UnknownFrom(in);
        }
        if (!filter) {
            return Support::Unexpected("filter " + std::to_string(i + 1) +
                                       " is not written the way filters are kept");
        }
        filters.push_back(std::move(*filter));
    }
    if (!in.Done())
        return Support::Unexpected(std::string("a filter list has numbers after its filters"));
    return filters;
}

}
