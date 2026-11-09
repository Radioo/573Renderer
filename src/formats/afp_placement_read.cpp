#include "formats/afp_animation.h"
#include "formats/afp_animation_detail.h"
#include "formats/afp_layout.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace AfpAnimation::Detail {

using namespace AfpLayout;

Filter ReadFilter(std::span<const uint8_t> bytes) {
    ByteReader r(bytes);
    const uint8_t type = bytes[0];
    if (type == kFilterColourMatrix &&
        (bytes.size() == kColourMatrixSize || bytes.size() == kColourMatrixHsvSize)) {
        ColourMatrixFilter filter;
        std::copy_n(bytes.begin(), kColourMatrixHeadSize, filter.head.begin());
        std::size_t at = kColourMatrixHeadSize;
        for (int32_t& value : filter.matrix) {
            value = r.S32(at);
            at += kIntSize;
        }
        if (bytes.size() == kColourMatrixHsvSize) {
            filter.hsv = Hsv{.hue = r.S16(kColourMatrixSize),
                             .saturation = static_cast<int8_t>(bytes[kColourMatrixSize + 2]),
                             .value = static_cast<int8_t>(bytes[kColourMatrixSize + 3])};
        }
        return filter;
    }
    if (type == kFilterLookup && bytes.size() >= kLookupTableField &&
        r.U16(kLookupLengthField) == bytes.size() - kLookupLengthBase) {
        LookupFilter filter;
        std::copy_n(bytes.begin(), kLookupHeadSize, filter.head.begin());
        std::copy_n(bytes.begin() + kLookupUnreadField, filter.unread_bytes.size(),
                    filter.unread_bytes.begin());
        filter.table.assign(bytes.begin() + kLookupTableField, bytes.end());
        return filter;
    }
    return UnknownFilter{.bytes = {bytes.begin(), bytes.end()}};
}

namespace {

class PlacementReader {
public:
    PlacementReader(std::span<const uint8_t> record, const StringTable& strings)
        : record_(record), r_(record), strings_(&strings) {}

    Support::Expected<Placement, std::string> Run() {
        const uint32_t flags = r_.U32(0);
        flags_ = flags;
        placement_.flags = flags & ~kPlacePresenceBits;
        placement_.depth = r_.U16(4);
        placement_.end_frame = r_.U16(6);
        pos_ = kPlacementHeaderSize;
        if (Has(kPlaceExtended)) {
            ext_ = U32();
            if ((ext_ & kExtControllerRecord) != 0) {
                return Support::Unexpected(
                    std::string("extended controller record 0x40 is not modelled"));
            }
            placement_.extended_flags = ext_ & ~kExtPresenceBits;
        }
        ReadIdentity();
        ReadMatrixAndColour();
        ReadBlocks();
        ReadOriginAndShortForms();
        ReadDepthFields();
        ReadExtendedFields();
        if (error_) return Support::Unexpected(std::move(*error_));
        if (r_.Failed()) return Support::Unexpected(std::string("placement is truncated"));
        auto tail = CheckTail(record_, pos_);
        if (!tail) return Support::Unexpected(tail.error());
        return std::move(placement_);
    }

private:
    [[nodiscard]] bool Has(uint32_t bit) const { return (flags_ & bit) != 0; }
    [[nodiscard]] bool HasExt(uint32_t bit) const { return (ext_ & bit) != 0; }

    void Fail(std::string message) {
        if (!error_) error_ = std::move(message);
    }

    uint16_t U16() {
        const uint16_t v = r_.U16(pos_);
        pos_ += 2;
        return v;
    }
    int16_t S16() { return static_cast<int16_t>(U16()); }
    uint32_t U32() {
        const uint32_t v = r_.U32(pos_);
        pos_ += kIntSize;
        return v;
    }
    int32_t S32() { return static_cast<int32_t>(U32()); }

    template <std::size_t N> std::array<int32_t, N> Ints() {
        std::array<int32_t, N> values{};
        for (int32_t& v : values)
            v = S32();
        return values;
    }

    template <std::size_t N> std::array<int16_t, N> Shorts() {
        std::array<int16_t, N> values{};
        for (int16_t& v : values)
            v = S16();
        return values;
    }

    std::optional<StringId> StringRef(const char* what) {
        auto id = strings_->Resolve(U16());
        if (!id) {
            Fail(std::string(what) + ": " + id.error());
            return std::nullopt;
        }
        return *id;
    }

    void Align() {
        const std::size_t aligned = AlignUp(pos_);
        for (; pos_ < aligned; pos_++) {
            if (r_.U8(pos_) != 0) Fail("placement padding is not zero");
        }
    }

    void ReadIdentity() {
        if (Has(kPlaceCharacter)) placement_.character = U16();
        if (Has(kPlaceRatio)) placement_.ratio = U16();
        if (Has(kPlaceName)) placement_.name = StringRef("placement name");
        if (Has(kPlaceClipDepth)) placement_.clip_depth = U16();
        if (Has(kPlaceBlend)) placement_.blend = r_.U8(pos_++);
        Align();
    }

    void ReadMatrixAndColour() {
        if (Has(kPlaceScale)) placement_.scale = Ints<2>();
        if (Has(kPlaceRotateSkew)) placement_.rotate_skew = Ints<2>();
        if (Has(kPlaceTranslation)) placement_.translation = Ints<2>();
        if (Has(kPlaceMultiplyColour)) placement_.multiply_colour = Shorts<4>();
        if (Has(kPlaceAddColour)) placement_.add_colour = Shorts<4>();
        if (Has(kPlacePackedMultiplyColour)) placement_.packed_multiply_colour = U32();
        if (Has(kPlacePackedAddColour)) placement_.packed_add_colour = U32();
    }

    void ReadBlocks() {
        if (Has(kPlaceClipActions)) ReadClipActions();
        if (Has(kPlaceFilters)) ReadFilters();
    }

    void ReadClipActions() {
        const std::size_t base = pos_;
        ClipActions clip{.unread_value = r_.U32(base), .unread_word = 0, .events = {}};
        const std::size_t size = r_.U32(base + kClipSizeField);
        clip.unread_word = r_.U16(base + kClipUnreadWordField);
        const std::size_t count = r_.U16(base + kClipEventCountField);
        const std::size_t events_end = kClipHeaderSize + (count * kClipEventSize);
        if (r_.Failed() || size < events_end || !r_.Has(base, size)) {
            Fail("clip action block is truncated");
            return;
        }
        std::size_t code_start = events_end;
        for (std::size_t i = 0; i < count; i++) {
            const std::size_t event = base + kClipHeaderSize + (i * kClipEventSize);
            const std::size_t start = event - base + r_.U16(event + kClipEventCodeField);
            const std::size_t next = event + kClipEventSize;
            const std::size_t end =
                i + 1 < count ? next - base + r_.U16(next + kClipEventCodeField) : size;
            if (start != code_start || end < start || end > size) {
                Fail("clip action bytecode is not laid out in event order");
                return;
            }
            auto bytecode = ReadBytecode(record_.subspan(base + start, end - start), *strings_);
            if (!bytecode) {
                Fail("clip action: " + bytecode.error());
                return;
            }
            clip.events.push_back(
                ClipEvent{.triggers = r_.U32(event),
                          .unread_bytes = {r_.U8(event + kClipEventBytesField),
                                           r_.U8(event + kClipEventBytesField + 1)},
                          .bytecode = std::move(*bytecode)});
            code_start = end;
        }
        if (code_start != size) {
            Fail("clip action block has bytes no event points at");
            return;
        }
        placement_.clip_actions = std::move(clip);
        pos_ = base + size;
    }

    void ReadFilters() {
        const std::size_t base = pos_;
        const std::size_t count = r_.U16(base);
        const std::size_t total = r_.U16(base + 2);
        const std::size_t header = AlignUp(kFilterListHeaderSize + (2 * count));
        if (r_.Failed() || total < header || !r_.Has(base, total)) {
            Fail("filter list is truncated");
            return;
        }
        if (count == 0 && total != header) {
            Fail("filter list has bytes no filter points at");
            return;
        }
        for (std::size_t i = base + kFilterListHeaderSize + (2 * count); i < base + header; i++) {
            if (r_.U8(i) != 0) Fail("filter list padding is not zero");
        }
        std::vector<Filter> filters;
        for (std::size_t i = 0; i < count; i++) {
            const std::size_t start = r_.U16(base + kFilterListHeaderSize + (2 * i));
            const std::size_t end =
                i + 1 < count ? r_.U16(base + kFilterListHeaderSize + (2 * (i + 1))) : total;
            const std::size_t expected = i == 0 ? header : start;
            if (start != expected || end <= start || end > total) {
                Fail("filters are not laid out in list order");
                return;
            }
            filters.push_back(ReadFilter(record_.subspan(base + start, end - start)));
        }
        placement_.filters = std::move(filters);
        pos_ = base + total;
    }

    void ReadOriginAndShortForms() {
        if (Has(kPlaceOrigin)) placement_.origin = Ints<2>();
        if (HasExt(kExtOriginZ)) placement_.origin_z = S32();
        if (Has(kPlaceGeometry)) placement_.geometry = U32();
        if (Has(kPlaceShortScale)) placement_.short_scale = Shorts<2>();
        if (Has(kPlaceShortRotateSkew)) placement_.short_rotate_skew = Shorts<2>();
        if (Has(kPlaceClassName)) placement_.class_name = StringRef("placement class name");
        Align();
    }

    void ReadDepthFields() {
        if (Has(kPlaceTranslationZ)) placement_.translation_z = S32();
        if (Has(kPlaceMatrix3d)) placement_.matrix_3d = Ints<kMatrix3dValues>();
        if (Has(kPlaceHsv)) {
            const int16_t hue = S16();
            const auto saturation = static_cast<int8_t>(r_.U8(pos_));
            const auto value = static_cast<int8_t>(r_.U8(pos_ + 1));
            pos_ += 2;
            placement_.hsv = Hsv{.hue = hue, .saturation = saturation, .value = value};
        }
    }

    void ReadExtendedFields() {
        if (HasExt(kExtDiscardedWords)) {
            DiscardedWords words{.mask = U32(), .values = {}};
            const int count = std::popcount(words.mask);
            for (int i = 0; i < count; i++)
                words.values.push_back(U16());
            placement_.discarded_words = std::move(words);
            Align();
        }
        if (HasExt(kExtCurves)) {
            ReadCurves();
            Align();
        }
        if (HasExt(kExtColourController)) {
            ColourController controller;
            const auto colour = r_.Bytes(pos_, kColourControllerBytes);
            if (!colour.empty())
                std::copy_n(colour.begin(), kColourControllerBytes, controller.colour.begin());
            pos_ += kColourControllerBytes;
            controller.first = S16();
            controller.second = S16();
            placement_.colour_controller = controller;
        }
        if (HasExt(kExtGridController)) {
            const uint16_t tag = U16();
            const int16_t first = S16();
            const int16_t second = S16();
            placement_.grid_controller =
                GridController{.tag = tag, .first = first, .second = second};
        }
    }

    void ReadCurves() {
        const uint32_t mask = U32();
        std::vector<Curve> curves;
        for (unsigned slot = 0; slot < kCurveSlots && !r_.Failed(); slot++) {
            if ((mask & (1U << slot)) == 0) continue;
            Curve curve{.slot = static_cast<uint8_t>(slot), .flags = U16(), .values = {}};
            const int16_t points = S16();
            const bool ints = (curve.flags & kCurveIntValues) != 0;
            const std::size_t per_point = (curve.flags & kCurveControlPoints) != 0
                                              ? kCurveControlPointValues
                                              : kCurvePointValues;
            const std::size_t width = ints ? kIntSize : 2;
            if (points < 0 || !r_.Has(pos_, static_cast<std::size_t>(points) * per_point * width)) {
                Fail("curve values run past the placement");
                return;
            }
            const std::size_t values = static_cast<std::size_t>(points) * per_point;
            for (std::size_t i = 0; i < values; i++)
                curve.values.push_back(ints ? S32() : S16());
            curves.push_back(std::move(curve));
        }
        placement_.curves = std::move(curves);
    }

    std::span<const uint8_t> record_;
    ByteReader r_;
    const StringTable* strings_;
    Placement placement_;
    uint32_t flags_ = 0;
    uint32_t ext_ = 0;
    std::size_t pos_ = 0;
    std::optional<std::string> error_;
};

}

Support::Expected<Placement, std::string> ReadPlacement(std::span<const uint8_t> record,
                                                        const StringTable& strings) {
    return PlacementReader(record, strings).Run();
}

}
