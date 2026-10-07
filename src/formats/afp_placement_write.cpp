#include "formats/afp_animation.h"
#include "formats/afp_animation_detail.h"
#include "formats/afp_layout.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <variant>
#include <vector>

namespace AfpAnimation::Detail {

namespace {

using namespace AfpLayout;

template <typename T> uint32_t Bit(const std::optional<T>& field, uint32_t bit) {
    return field ? bit : 0U;
}

uint32_t PresenceFlags(const Placement& p) {
    return Bit(p.character, kPlaceCharacter) | Bit(p.ratio, kPlaceRatio) | Bit(p.name, kPlaceName) |
           Bit(p.clip_depth, kPlaceClipDepth) | Bit(p.blend, kPlaceBlend) |
           Bit(p.scale, kPlaceScale) | Bit(p.rotate_skew, kPlaceRotateSkew) |
           Bit(p.translation, kPlaceTranslation) | Bit(p.multiply_colour, kPlaceMultiplyColour) |
           Bit(p.add_colour, kPlaceAddColour) |
           Bit(p.packed_multiply_colour, kPlacePackedMultiplyColour) |
           Bit(p.packed_add_colour, kPlacePackedAddColour) |
           Bit(p.clip_actions, kPlaceClipActions) | Bit(p.filters, kPlaceFilters) |
           Bit(p.origin, kPlaceOrigin) | Bit(p.geometry, kPlaceGeometry) |
           Bit(p.short_scale, kPlaceShortScale) | Bit(p.short_rotate_skew, kPlaceShortRotateSkew) |
           Bit(p.class_name, kPlaceClassName) | Bit(p.translation_z, kPlaceTranslationZ) |
           Bit(p.matrix_3d, kPlaceMatrix3d) | Bit(p.hsv, kPlaceHsv);
}

uint32_t ExtendedPresenceFlags(const Placement& p) {
    return Bit(p.origin_z, kExtOriginZ) | Bit(p.discarded_words, kExtDiscardedWords) |
           Bit(p.curves, kExtCurves) | Bit(p.colour_controller, kExtColourController) |
           Bit(p.grid_controller, kExtGridController);
}

template <std::size_t N>
void WriteInts(ByteWriter& out, const std::optional<std::array<int32_t, N>>& values) {
    if (!values) return;
    for (const int32_t v : *values)
        out.S32(v);
}

template <std::size_t N>
void WriteShorts(ByteWriter& out, const std::optional<std::array<int16_t, N>>& values) {
    if (!values) return;
    for (const int16_t v : *values)
        out.S16(v);
}

void WriteHsv(ByteWriter& out, const Hsv& hsv) {
    out.S16(hsv.hue);
    out.U8(static_cast<uint8_t>(hsv.saturation));
    out.U8(static_cast<uint8_t>(hsv.value));
}

void WriteClipActions(ByteWriter& out, const ClipActions& clip) {
    const std::size_t base = out.Position();
    if (clip.events.size() > kMaxWordValue) out.Fail("more than 65535 clip events");
    out.U32(clip.unread_value);
    out.U32(0);
    out.U16(clip.unread_word);
    out.U16(static_cast<uint16_t>(clip.events.size()));
    std::vector<std::size_t> heads;
    for (const ClipEvent& event : clip.events) {
        heads.push_back(out.Position());
        out.U32(event.triggers);
        out.U8(event.unread_bytes[0]);
        out.U8(event.unread_bytes[1]);
        out.U16(0);
    }
    for (std::size_t i = 0; i < clip.events.size(); i++) {
        const std::size_t offset = out.Position() - heads[i];
        if (offset > kMaxWordValue) out.Fail("clip event bytecode lies beyond a 16-bit offset");
        out.PatchU16(heads[i] + kClipEventCodeField, static_cast<uint16_t>(offset));
        WriteBytecode(out, clip.events[i].bytecode);
    }
    out.PatchU32(base + kClipSizeField, static_cast<uint32_t>(out.Position() - base));
}

void WriteFilter(ByteWriter& out, const Filter& filter) {
    if (const auto* matrix = std::get_if<ColourMatrixFilter>(&filter)) {
        if (matrix->head[0] != kFilterColourMatrix)
            out.Fail(std::format("colour matrix filter has type byte {:#x}", matrix->head[0]));
        out.Raw(matrix->head);
        for (const int32_t v : matrix->matrix)
            out.S32(v);
        if (matrix->hsv) WriteHsv(out, *matrix->hsv);
    } else if (const auto* lookup = std::get_if<LookupFilter>(&filter)) {
        if (lookup->head[0] != kFilterLookup)
            out.Fail(std::format("lookup filter has type byte {:#x}", lookup->head[0]));
        out.Raw(lookup->head);
        const std::size_t length = lookup->table.size() + kLookupTableField - kLookupLengthBase;
        if (length > kMaxWordValue) out.Fail("lookup filter table is too large");
        out.U16(static_cast<uint16_t>(length));
        out.Raw(lookup->unread_bytes);
        out.Raw(lookup->table);
    } else if (const auto* unknown = std::get_if<UnknownFilter>(&filter)) {
        if (unknown->bytes.empty() ||
            !std::holds_alternative<UnknownFilter>(ReadFilter(unknown->bytes))) {
            out.Fail(
                "an unknown filter must be non-empty and must not read back as a modelled filter");
        }
        out.OpaqueRaw(unknown->bytes, "an unmodelled filter");
    }
}

void WriteFilters(ByteWriter& out, const std::vector<Filter>& filters) {
    const std::size_t base = out.Position();
    const std::size_t count = filters.size();
    const std::size_t header = AlignUp(kFilterListHeaderSize + (2 * count));
    if (count > kMaxWordValue) out.Fail("more than 65535 filters");
    out.U16(static_cast<uint16_t>(count));
    out.U16(0);
    for (std::size_t pos = kFilterListHeaderSize; pos < header; pos += 2)
        out.U16(0);
    for (std::size_t i = 0; i < count; i++) {
        out.PatchU16(base + kFilterListHeaderSize + (2 * i),
                     static_cast<uint16_t>(out.Position() - base));
        WriteFilter(out, filters[i]);
    }
    const std::size_t total = out.Position() - base;
    if (total > kMaxWordValue) out.Fail("filter list exceeds 65535 bytes");
    out.PatchU16(base + 2, static_cast<uint16_t>(total));
}

void WriteDiscardedWords(ByteWriter& out, const DiscardedWords& words) {
    if (static_cast<std::size_t>(std::popcount(words.mask)) != words.values.size())
        out.Fail("discarded word count does not match its mask");
    out.U32(words.mask);
    for (const uint16_t v : words.values)
        out.U16(v);
    out.Align4();
}

void WriteCurves(ByteWriter& out, const std::vector<Curve>& curves) {
    uint32_t mask = 0;
    std::optional<uint8_t> previous;
    for (const Curve& curve : curves) {
        if (curve.slot >= kCurveSlots || (previous && curve.slot <= *previous)) {
            out.Fail("curve slots must be ascending and below 32");
        }
        previous = curve.slot;
        mask |= 1U << (curve.slot % kCurveSlots);
    }
    out.U32(mask);
    for (const Curve& curve : curves) {
        const std::size_t per_point =
            (curve.flags & kCurveControlPoints) != 0 ? kCurveControlPointValues : kCurvePointValues;
        const std::size_t points = curve.values.size() / per_point;
        if (curve.values.size() % per_point != 0 ||
            points > static_cast<std::size_t>(std::numeric_limits<int16_t>::max())) {
            out.Fail(
                std::format("curve in slot {} has {} values", curve.slot, curve.values.size()));
        }
        out.U16(curve.flags);
        out.S16(static_cast<int16_t>(points));
        const bool ints = (curve.flags & kCurveIntValues) != 0;
        for (const int32_t v : curve.values) {
            if (ints) {
                out.S32(v);
                continue;
            }
            if (v < std::numeric_limits<int16_t>::min() || v > std::numeric_limits<int16_t>::max())
                out.Fail(std::format("curve value {} does not fit 16 bits", v));
            out.S16(static_cast<int16_t>(v));
        }
    }
    out.Align4();
}

void WriteExtendedFields(ByteWriter& out, const Placement& p) {
    if (p.discarded_words) WriteDiscardedWords(out, *p.discarded_words);
    if (p.curves) WriteCurves(out, *p.curves);
    if (p.colour_controller) {
        out.Raw(p.colour_controller->colour);
        out.S16(p.colour_controller->first);
        out.S16(p.colour_controller->second);
    }
    if (p.grid_controller) {
        out.U16(p.grid_controller->tag);
        out.S16(p.grid_controller->first);
        out.S16(p.grid_controller->second);
    }
}

}

void WritePlacement(ByteWriter& out, const Placement& p) {
    if ((p.flags & kPlacePresenceBits) != 0)
        out.Fail(std::format("placement flags {:#x} hold field presence bits", p.flags));
    if (p.extended_flags && (*p.extended_flags & (kExtPresenceBits | kExtControllerRecord)) != 0)
        out.Fail(std::format("extended flags {:#x} hold field presence bits", *p.extended_flags));
    if (!p.extended_flags && ExtendedPresenceFlags(p) != 0)
        out.Fail("extended placement fields need extended_flags to be set");
    const uint32_t ext = p.extended_flags.value_or(0) | ExtendedPresenceFlags(p);
    const bool has_ext = p.extended_flags.has_value();
    out.U32(p.flags | PresenceFlags(p) | (has_ext ? kPlaceExtended : 0U));
    out.U16(p.depth);
    out.U16(p.end_frame);
    if (has_ext) out.U32(ext);
    if (p.character) out.U16(*p.character);
    if (p.ratio) out.U16(*p.ratio);
    if (p.name) out.StringRef(*p.name);
    if (p.clip_depth) out.U16(*p.clip_depth);
    if (p.blend) out.U8(*p.blend);
    out.Align4();
    WriteInts(out, p.scale);
    WriteInts(out, p.rotate_skew);
    WriteInts(out, p.translation);
    WriteShorts(out, p.multiply_colour);
    WriteShorts(out, p.add_colour);
    if (p.packed_multiply_colour) out.U32(*p.packed_multiply_colour);
    if (p.packed_add_colour) out.U32(*p.packed_add_colour);
    if (p.clip_actions) WriteClipActions(out, *p.clip_actions);
    if (p.filters) WriteFilters(out, *p.filters);
    WriteInts(out, p.origin);
    if (p.origin_z) out.S32(*p.origin_z);
    if (p.geometry) out.U32(*p.geometry);
    WriteShorts(out, p.short_scale);
    WriteShorts(out, p.short_rotate_skew);
    if (p.class_name) out.StringRef(*p.class_name);
    out.Align4();
    if (p.translation_z) out.S32(*p.translation_z);
    WriteInts(out, p.matrix_3d);
    if (p.hsv) WriteHsv(out, *p.hsv);
    WriteExtendedFields(out, p);
}

}
