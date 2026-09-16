#include "document/placement_values.h"

#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

namespace {

enum class PropertyId : uint8_t {
    Ratio,
    ClipDepth,
    Blend,
    Scale,
    RotateSkew,
    Translation,
    MultiplyColour,
    AddColour,
    PackedMultiplyColour,
    PackedAddColour,
    Origin,
    OriginZ,
    ShortScale,
    ShortRotateSkew,
    TranslationZ,
    Matrix3d,
    Hsv,
};

struct NamedProperty {
    std::string_view name;
    PropertyId id;
};

constexpr std::array<NamedProperty, 17> kProperties{{
    {.name = "Ratio", .id = PropertyId::Ratio},
    {.name = "Clip depth", .id = PropertyId::ClipDepth},
    {.name = "Blend", .id = PropertyId::Blend},
    {.name = "Scale", .id = PropertyId::Scale},
    {.name = "Rotate skew", .id = PropertyId::RotateSkew},
    {.name = "Translation", .id = PropertyId::Translation},
    {.name = "Multiply colour", .id = PropertyId::MultiplyColour},
    {.name = "Add colour", .id = PropertyId::AddColour},
    {.name = "Packed multiply colour", .id = PropertyId::PackedMultiplyColour},
    {.name = "Packed add colour", .id = PropertyId::PackedAddColour},
    {.name = "Origin", .id = PropertyId::Origin},
    {.name = "Origin z", .id = PropertyId::OriginZ},
    {.name = "Short scale", .id = PropertyId::ShortScale},
    {.name = "Short rotate skew", .id = PropertyId::ShortRotateSkew},
    {.name = "Translation z", .id = PropertyId::TranslationZ},
    {.name = "3D matrix", .id = PropertyId::Matrix3d},
    {.name = "HSV", .id = PropertyId::Hsv},
}};

constexpr auto kPropertyNames = [] {
    std::array<std::string_view, kProperties.size()> out{};
    for (std::size_t i = 0; i < kProperties.size(); i++)
        out[i] = kProperties[i].name;
    return out;
}();

std::optional<PropertyId> PropertyFor(std::string_view name) {
    const auto found = std::ranges::find(kProperties, name, &NamedProperty::name);
    if (found == kProperties.end()) return std::nullopt;
    return found->id;
}

template <typename T> std::optional<std::vector<int64_t>> Read(const std::optional<T>& field) {
    if (!field) return std::nullopt;
    return std::vector<int64_t>{static_cast<int64_t>(*field)};
}

template <typename T, std::size_t N>
std::optional<std::vector<int64_t>> Read(const std::optional<std::array<T, N>>& field) {
    if (!field) return std::nullopt;
    std::vector<int64_t> out;
    out.reserve(N);
    for (const T value : *field)
        out.push_back(static_cast<int64_t>(value));
    return out;
}

std::optional<std::vector<int64_t>> Read(const std::optional<AfpAnimation::Hsv>& field) {
    if (!field) return std::nullopt;
    return std::vector<int64_t>{field->hue, field->saturation, field->value};
}

template <typename T> bool Fits(int64_t value) {
    return value >= static_cast<int64_t>(std::numeric_limits<T>::min()) &&
           value <= static_cast<int64_t>(std::numeric_limits<T>::max());
}

template <typename T>
Support::Expected<void, std::string> Write(std::optional<T>& field,
                                           std::span<const int64_t> value) {
    if (value.size() != 1)
        return Support::Unexpected(std::string("that property holds one number"));
    if (!Fits<T>(value[0]))
        return Support::Unexpected(std::to_string(value[0]) + " does not fit the field");
    field = static_cast<T>(value[0]);
    return {};
}

template <typename T, std::size_t N>
Support::Expected<void, std::string> Write(std::optional<std::array<T, N>>& field,
                                           std::span<const int64_t> value) {
    if (value.size() != N) {
        return Support::Unexpected("that property holds " + std::to_string(N) + " numbers, not " +
                                   std::to_string(value.size()));
    }
    std::array<T, N> out{};
    for (std::size_t i = 0; i < N; i++) {
        if (!Fits<T>(value[i]))
            return Support::Unexpected(std::to_string(value[i]) + " does not fit the field");
        out[i] = static_cast<T>(value[i]);
    }
    field = out;
    return {};
}

Support::Expected<void, std::string> Write(std::optional<AfpAnimation::Hsv>& field,
                                           std::span<const int64_t> value) {
    if (value.size() != 3) return Support::Unexpected(std::string("HSV holds three numbers"));
    if (!Fits<int16_t>(value[0]) || !Fits<int8_t>(value[1]) || !Fits<int8_t>(value[2]))
        return Support::Unexpected(std::string("an HSV value does not fit the field"));
    field = AfpAnimation::Hsv{.hue = static_cast<int16_t>(value[0]),
                              .saturation = static_cast<int8_t>(value[1]),
                              .value = static_cast<int8_t>(value[2])};
    return {};
}

}

std::span<const std::string_view> AnimatableProperties() {
    return kPropertyNames;
}

bool PropertyIsAnimatable(std::string_view name) {
    return PropertyFor(name).has_value();
}

std::optional<std::vector<int64_t>> ReadProperty(const AfpAnimation::Placement& placement,
                                                 std::string_view name) {
    const std::optional<PropertyId> id = PropertyFor(name);
    if (!id) return std::nullopt;
    switch (*id) {
    case PropertyId::Ratio:
        return Read(placement.ratio);
    case PropertyId::ClipDepth:
        return Read(placement.clip_depth);
    case PropertyId::Blend:
        return Read(placement.blend);
    case PropertyId::Scale:
        return Read(placement.scale);
    case PropertyId::RotateSkew:
        return Read(placement.rotate_skew);
    case PropertyId::Translation:
        return Read(placement.translation);
    case PropertyId::MultiplyColour:
        return Read(placement.multiply_colour);
    case PropertyId::AddColour:
        return Read(placement.add_colour);
    case PropertyId::PackedMultiplyColour:
        return Read(placement.packed_multiply_colour);
    case PropertyId::PackedAddColour:
        return Read(placement.packed_add_colour);
    case PropertyId::Origin:
        return Read(placement.origin);
    case PropertyId::OriginZ:
        return Read(placement.origin_z);
    case PropertyId::ShortScale:
        return Read(placement.short_scale);
    case PropertyId::ShortRotateSkew:
        return Read(placement.short_rotate_skew);
    case PropertyId::TranslationZ:
        return Read(placement.translation_z);
    case PropertyId::Matrix3d:
        return Read(placement.matrix_3d);
    case PropertyId::Hsv:
        return Read(placement.hsv);
    }
    return std::nullopt;
}

Support::Expected<void, std::string> WriteProperty(AfpAnimation::Placement& placement,
                                                   std::string_view name,
                                                   std::span<const int64_t> value) {
    const std::optional<PropertyId> id = PropertyFor(name);
    if (!id) return Support::Unexpected(std::string(name) + " is not an animatable property");
    switch (*id) {
    case PropertyId::Ratio:
        return Write(placement.ratio, value);
    case PropertyId::ClipDepth:
        return Write(placement.clip_depth, value);
    case PropertyId::Blend:
        return Write(placement.blend, value);
    case PropertyId::Scale:
        return Write(placement.scale, value);
    case PropertyId::RotateSkew:
        return Write(placement.rotate_skew, value);
    case PropertyId::Translation:
        return Write(placement.translation, value);
    case PropertyId::MultiplyColour:
        return Write(placement.multiply_colour, value);
    case PropertyId::AddColour:
        return Write(placement.add_colour, value);
    case PropertyId::PackedMultiplyColour:
        return Write(placement.packed_multiply_colour, value);
    case PropertyId::PackedAddColour:
        return Write(placement.packed_add_colour, value);
    case PropertyId::Origin:
        return Write(placement.origin, value);
    case PropertyId::OriginZ:
        return Write(placement.origin_z, value);
    case PropertyId::ShortScale:
        return Write(placement.short_scale, value);
    case PropertyId::ShortRotateSkew:
        return Write(placement.short_rotate_skew, value);
    case PropertyId::TranslationZ:
        return Write(placement.translation_z, value);
    case PropertyId::Matrix3d:
        return Write(placement.matrix_3d, value);
    case PropertyId::Hsv:
        return Write(placement.hsv, value);
    }
    return Support::Unexpected(std::string(name) + " is not an animatable property");
}

void ClearAnimatableProperties(AfpAnimation::Placement& placement) {
    placement.ratio.reset();
    placement.clip_depth.reset();
    placement.blend.reset();
    placement.scale.reset();
    placement.rotate_skew.reset();
    placement.translation.reset();
    placement.multiply_colour.reset();
    placement.add_colour.reset();
    placement.packed_multiply_colour.reset();
    placement.packed_add_colour.reset();
    placement.origin.reset();
    placement.origin_z.reset();
    placement.short_scale.reset();
    placement.short_rotate_skew.reset();
    placement.translation_z.reset();
    placement.matrix_3d.reset();
    placement.hsv.reset();
}

std::vector<std::string> UnanimatableParts(const AfpAnimation::Placement& placement) {
    std::vector<std::string> parts;
    if (placement.character) parts.emplace_back("a character");
    if (placement.name) parts.emplace_back("an instance name");
    if (placement.class_name) parts.emplace_back("a class name");
    if (placement.geometry) parts.emplace_back("a geometry");
    if (placement.clip_actions) parts.emplace_back("clip actions");
    if (placement.filters) parts.emplace_back("filters");
    if (placement.discarded_words) parts.emplace_back("discarded words");
    if (placement.curves) parts.emplace_back("curves");
    if (placement.colour_controller) parts.emplace_back("a colour controller");
    if (placement.grid_controller) parts.emplace_back("a grid controller");
    return parts;
}

}
