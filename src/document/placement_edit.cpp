#include "document/placement_edit.h"

#include "document/animation_strings.h"
#include "document/outline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <system_error>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr uint32_t kUpdateExisting = 0x1;

enum class FieldId : uint8_t {
    Character,
    Ratio,
    Name,
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
    Geometry,
    ShortScale,
    ShortRotateSkew,
    ClassName,
    TranslationZ,
    Matrix3d,
    Hsv,
};

struct NamedField {
    std::string_view name;
    FieldId id;
};

constexpr std::array<NamedField, 20> kFields{{
    {.name = "Character", .id = FieldId::Character},
    {.name = "Ratio", .id = FieldId::Ratio},
    {.name = "Name", .id = FieldId::Name},
    {.name = "Clip depth", .id = FieldId::ClipDepth},
    {.name = "Blend", .id = FieldId::Blend},
    {.name = "Scale", .id = FieldId::Scale},
    {.name = "Rotate skew", .id = FieldId::RotateSkew},
    {.name = "Translation", .id = FieldId::Translation},
    {.name = "Multiply colour", .id = FieldId::MultiplyColour},
    {.name = "Add colour", .id = FieldId::AddColour},
    {.name = "Packed multiply colour", .id = FieldId::PackedMultiplyColour},
    {.name = "Packed add colour", .id = FieldId::PackedAddColour},
    {.name = "Origin", .id = FieldId::Origin},
    {.name = "Geometry", .id = FieldId::Geometry},
    {.name = "Short scale", .id = FieldId::ShortScale},
    {.name = "Short rotate skew", .id = FieldId::ShortRotateSkew},
    {.name = "Class name", .id = FieldId::ClassName},
    {.name = "Translation z", .id = FieldId::TranslationZ},
    {.name = "3D matrix", .id = FieldId::Matrix3d},
    {.name = "HSV", .id = FieldId::Hsv},
}};

std::optional<FieldId> FieldFor(std::string_view name) {
    const auto found = std::ranges::find(kFields, name, &NamedField::name);
    if (found == kFields.end()) return std::nullopt;
    return found->id;
}

std::string Join(const std::vector<std::string>& parts) {
    std::string out;
    for (const std::string& part : parts) {
        if (!out.empty()) out += ", ";
        out += part;
    }
    return out;
}

template <typename T> std::string Scalar(const std::optional<T>& field) {
    return field ? std::to_string(*field) : std::string();
}

template <typename T, std::size_t N>
std::string Vector(const std::optional<std::array<T, N>>& field) {
    if (!field) return {};
    std::vector<std::string> parts;
    parts.reserve(N);
    for (const T value : *field)
        parts.push_back(std::to_string(value));
    return Join(parts);
}

std::string HsvText(const std::optional<AfpAnimation::Hsv>& field) {
    if (!field) return {};
    return Join({std::to_string(field->hue), std::to_string(field->saturation),
                 std::to_string(field->value)});
}

std::string NameText(const AfpAnimation::Animation& animation,
                     const std::optional<AfpAnimation::StringId>& field) {
    if (!field) return {};
    return StringText(animation, *field);
}

Support::Expected<std::vector<int64_t>, std::string> Numbers(std::string_view value,
                                                             std::size_t count) {
    std::vector<int64_t> out;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t comma = value.find(',', start);
        std::string_view part = value.substr(
            start, comma == std::string_view::npos ? value.size() - start : comma - start);
        while (!part.empty() && part.front() == ' ')
            part.remove_prefix(1);
        while (!part.empty() && part.back() == ' ')
            part.remove_suffix(1);
        int64_t number = 0;
        const auto* end = part.data() + part.size();
        const auto parsed = std::from_chars(part.data(), end, number);
        if (parsed.ec != std::errc{} || parsed.ptr != end)
            return Support::Unexpected("not a number: " + std::string(part));
        out.push_back(number);
        if (comma == std::string_view::npos) break;
        start = comma + 1;
    }
    if (out.size() != count) {
        return Support::Unexpected("expected " + std::to_string(count) + " numbers, got " +
                                   std::to_string(out.size()));
    }
    return out;
}

template <typename T>
Support::Expected<void, std::string> SetScalar(std::optional<T>& field, std::string_view value) {
    if (value.empty()) {
        field.reset();
        return {};
    }
    auto numbers = Numbers(value, 1);
    if (!numbers) return Support::Unexpected(numbers.error());
    const int64_t number = numbers->front();
    if (number < static_cast<int64_t>(std::numeric_limits<T>::min()) ||
        number > static_cast<int64_t>(std::numeric_limits<T>::max())) {
        return Support::Unexpected(std::to_string(number) + " does not fit the field");
    }
    field = static_cast<T>(number);
    return {};
}

template <typename T, std::size_t N>
Support::Expected<void, std::string> SetVector(std::optional<std::array<T, N>>& field,
                                               std::string_view value) {
    if (value.empty()) {
        field.reset();
        return {};
    }
    auto numbers = Numbers(value, N);
    if (!numbers) return Support::Unexpected(numbers.error());
    std::array<T, N> out{};
    for (std::size_t i = 0; i < N; i++) {
        const int64_t number = (*numbers)[i];
        if (number < static_cast<int64_t>(std::numeric_limits<T>::min()) ||
            number > static_cast<int64_t>(std::numeric_limits<T>::max())) {
            return Support::Unexpected(std::to_string(number) + " does not fit the field");
        }
        out[i] = static_cast<T>(number);
    }
    field = out;
    return {};
}

Support::Expected<void, std::string> SetHsv(std::optional<AfpAnimation::Hsv>& field,
                                            std::string_view value) {
    if (value.empty()) {
        field.reset();
        return {};
    }
    auto numbers = Numbers(value, 3);
    if (!numbers) return Support::Unexpected(numbers.error());
    const int64_t hue = (*numbers)[0];
    const int64_t saturation = (*numbers)[1];
    const int64_t brightness = (*numbers)[2];
    if (hue < std::numeric_limits<int16_t>::min() || hue > std::numeric_limits<int16_t>::max() ||
        saturation < std::numeric_limits<int8_t>::min() ||
        saturation > std::numeric_limits<int8_t>::max() ||
        brightness < std::numeric_limits<int8_t>::min() ||
        brightness > std::numeric_limits<int8_t>::max()) {
        return Support::Unexpected(std::string("an HSV value does not fit the field"));
    }
    field = AfpAnimation::Hsv{.hue = static_cast<int16_t>(hue),
                              .saturation = static_cast<int8_t>(saturation),
                              .value = static_cast<int8_t>(brightness)};
    return {};
}

Support::Expected<void, std::string> SetName(AfpAnimation::Animation& animation,
                                             std::optional<AfpAnimation::StringId>& field,
                                             std::string_view value) {
    if (value.empty()) {
        field.reset();
        return {};
    }
    field = InternString(animation, value);
    return {};
}

std::string FieldText(const AfpAnimation::Animation& animation,
                      const AfpAnimation::Placement& placement, FieldId id) {
    switch (id) {
    case FieldId::Character:
        return Scalar(placement.character);
    case FieldId::Ratio:
        return Scalar(placement.ratio);
    case FieldId::Name:
        return NameText(animation, placement.name);
    case FieldId::ClipDepth:
        return Scalar(placement.clip_depth);
    case FieldId::Blend:
        return Scalar(placement.blend);
    case FieldId::Scale:
        return Vector(placement.scale);
    case FieldId::RotateSkew:
        return Vector(placement.rotate_skew);
    case FieldId::Translation:
        return Vector(placement.translation);
    case FieldId::MultiplyColour:
        return Vector(placement.multiply_colour);
    case FieldId::AddColour:
        return Vector(placement.add_colour);
    case FieldId::PackedMultiplyColour:
        return Scalar(placement.packed_multiply_colour);
    case FieldId::PackedAddColour:
        return Scalar(placement.packed_add_colour);
    case FieldId::Origin:
        return Vector(placement.origin);
    case FieldId::Geometry:
        return Scalar(placement.geometry);
    case FieldId::ShortScale:
        return Vector(placement.short_scale);
    case FieldId::ShortRotateSkew:
        return Vector(placement.short_rotate_skew);
    case FieldId::ClassName:
        return NameText(animation, placement.class_name);
    case FieldId::TranslationZ:
        return Scalar(placement.translation_z);
    case FieldId::Matrix3d:
        return Vector(placement.matrix_3d);
    case FieldId::Hsv:
        return HsvText(placement.hsv);
    }
    return {};
}

Support::Expected<void, std::string> SetField(AfpAnimation::Animation& animation,
                                              AfpAnimation::Placement& placement, FieldId id,
                                              std::string_view value) {
    switch (id) {
    case FieldId::Character:
        return SetScalar(placement.character, value);
    case FieldId::Ratio:
        return SetScalar(placement.ratio, value);
    case FieldId::Name:
        return SetName(animation, placement.name, value);
    case FieldId::ClipDepth:
        return SetScalar(placement.clip_depth, value);
    case FieldId::Blend:
        return SetScalar(placement.blend, value);
    case FieldId::Scale:
        return SetVector(placement.scale, value);
    case FieldId::RotateSkew:
        return SetVector(placement.rotate_skew, value);
    case FieldId::Translation:
        return SetVector(placement.translation, value);
    case FieldId::MultiplyColour:
        return SetVector(placement.multiply_colour, value);
    case FieldId::AddColour:
        return SetVector(placement.add_colour, value);
    case FieldId::PackedMultiplyColour:
        return SetScalar(placement.packed_multiply_colour, value);
    case FieldId::PackedAddColour:
        return SetScalar(placement.packed_add_colour, value);
    case FieldId::Origin:
        return SetVector(placement.origin, value);
    case FieldId::Geometry:
        return SetScalar(placement.geometry, value);
    case FieldId::ShortScale:
        return SetVector(placement.short_scale, value);
    case FieldId::ShortRotateSkew:
        return SetVector(placement.short_rotate_skew, value);
    case FieldId::ClassName:
        return SetName(animation, placement.class_name, value);
    case FieldId::TranslationZ:
        return SetScalar(placement.translation_z, value);
    case FieldId::Matrix3d:
        return SetVector(placement.matrix_3d, value);
    case FieldId::Hsv:
        return SetHsv(placement.hsv, value);
    }
    return Support::Unexpected(std::string("unknown placement field"));
}

std::string UnknownText(const AfpAnimation::Placement& placement) {
    std::vector<std::string> parts;
    if (placement.clip_actions) parts.emplace_back("clip actions");
    if (placement.filters) parts.emplace_back("filters");
    if (placement.origin_z) parts.emplace_back("origin z");
    if (placement.discarded_words) parts.emplace_back("discarded words");
    if (placement.curves) parts.emplace_back("curves");
    if (placement.colour_controller) parts.emplace_back("colour controller");
    if (placement.grid_controller) parts.emplace_back("grid controller");
    return Join(parts);
}

}

std::optional<std::size_t> LivePlacementTag(const AfpAnimation::Container& clip, uint16_t depth,
                                            uint32_t frame) {
    std::optional<std::size_t> live;
    for (std::size_t index = 0; index < clip.frames.size() && index <= frame; index++) {
        const AfpAnimation::Frame& current = clip.frames[index];
        for (uint32_t tag = 0; tag < current.tag_count; tag++) {
            const std::size_t position = current.first_tag + tag;
            if (position >= clip.tags.size()) break;
            const auto& body = clip.tags[position].body;
            if (const auto* placement = std::get_if<AfpAnimation::Placement>(&body)) {
                if (placement->depth == depth) live = position;
            } else if (const auto* remove = std::get_if<AfpAnimation::Remove>(&body)) {
                if (remove->depth == depth) live.reset();
            }
        }
    }
    return live;
}

std::vector<Field> PlacementFields(const AfpAnimation::Animation& animation,
                                   const AfpAnimation::Placement& placement) {
    std::vector<Field> fields;
    fields.push_back(Field{.name = "Depth", .value = std::to_string(placement.depth)});
    fields.push_back(Field{.name = "End frame", .value = std::to_string(placement.end_frame)});
    fields.push_back(Field{.name = "Updates the depth",
                           .value = (placement.flags & kUpdateExisting) != 0 ? "yes" : "no"});
    for (const NamedField& named : kFields) {
        fields.push_back(Field{.name = std::string(named.name),
                               .value = FieldText(animation, placement, named.id)});
    }
    const std::string unknown = UnknownText(placement);
    if (!unknown.empty()) fields.push_back(Field{.name = "Unknown data", .value = unknown});
    return fields;
}

bool PlacementFieldIsEditable(std::string_view name) {
    return FieldFor(name).has_value();
}

Support::Expected<void, std::string> SetPlacementField(AfpAnimation::Animation& animation,
                                                       AfpAnimation::Placement& placement,
                                                       std::string_view name,
                                                       std::string_view value) {
    const std::optional<FieldId> id = FieldFor(name);
    if (!id) return Support::Unexpected(std::string(name) + " is not an editable placement field");
    return SetField(animation, placement, *id, value);
}

}
