#include "document/curve_values.h"

#include "document/number_reader.h"
#include "formats/afp_animation.h"
#include "formats/afp_layout.h"
#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Document {

namespace {

std::size_t PointWidth(uint16_t flags) {
    return (flags & AfpLayout::kCurveControlPoints) != 0 ? AfpLayout::kCurveControlPointValues
                                                         : AfpLayout::kCurvePointValues;
}

std::size_t Points(const AfpAnimation::Curve& curve) {
    return curve.values.size() / PointWidth(curve.flags);
}

std::optional<AfpAnimation::Curve> CurveFrom(NumberReader& in) {
    const std::optional<uint8_t> slot = in.Next<uint8_t>();
    const std::optional<uint16_t> flags = in.Next<uint16_t>();
    const std::optional<uint32_t> count = in.Next<uint32_t>();
    if (!slot || !flags || !count || *slot >= AfpLayout::kCurveSlots || *count > in.Left() ||
        *count % PointWidth(*flags) != 0)
        return std::nullopt;
    AfpAnimation::Curve curve{.slot = *slot, .flags = *flags, .values = {}};
    curve.values.reserve(*count);
    const bool ints = (*flags & AfpLayout::kCurveIntValues) != 0;
    for (uint32_t i = 0; i < *count; i++) {
        const std::optional<int32_t> value =
            ints ? in.Next<int32_t>() : std::optional<int32_t>(in.Next<int16_t>());
        if (!value) return std::nullopt;
        curve.values.push_back(*value);
    }
    return curve;
}

}

std::vector<int64_t> CurveNumbers(const std::vector<AfpAnimation::Curve>& curves) {
    std::vector<int64_t> out{static_cast<int64_t>(curves.size())};
    for (const AfpAnimation::Curve& curve : curves) {
        out.insert(out.end(), {curve.slot, curve.flags, static_cast<int64_t>(curve.values.size())});
        out.insert(out.end(), curve.values.begin(), curve.values.end());
    }
    return out;
}

Support::Expected<std::vector<AfpAnimation::Curve>, std::string>
CurvesFrom(std::span<const int64_t> numbers) {
    NumberReader in(numbers);
    const std::optional<uint8_t> count = in.Next<uint8_t>();
    if (!count || *count > AfpLayout::kCurveSlots)
        return Support::Unexpected(std::string("a curve list starts with its count"));
    std::vector<AfpAnimation::Curve> curves;
    for (uint8_t i = 0; i < *count; i++) {
        std::optional<AfpAnimation::Curve> curve = CurveFrom(in);
        if (!curve || (!curves.empty() && curve->slot <= curves.back().slot)) {
            return Support::Unexpected("curve " + std::to_string(i + 1) +
                                       " is not written the way curves are kept");
        }
        curves.push_back(std::move(*curve));
    }
    if (!in.Done())
        return Support::Unexpected(std::string("a curve list has numbers after its curves"));
    return curves;
}

Support::Expected<void, std::string> CheckCurvesFit(const std::vector<AfpAnimation::Curve>& first,
                                                    const std::vector<AfpAnimation::Curve>& later) {
    for (const AfpAnimation::Curve& curve : later) {
        const std::string slot = "curve slot " + std::to_string(curve.slot);
        if (curve.slot >= first.size()) {
            return Support::Unexpected(slot + " is past the " + std::to_string(first.size()) +
                                       " curves the depth's first curve set made room for");
        }
        const std::size_t room = Points(first[curve.slot]);
        if (Points(curve) > room) {
            return Support::Unexpected(slot + " has " + std::to_string(Points(curve)) +
                                       " points, more than the " + std::to_string(room) +
                                       " the depth's first curve set made room for");
        }
    }
    return {};
}

}
