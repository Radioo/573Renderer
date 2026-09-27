#include "formats/afp_animation.h"

#include "formats/afp_layout.h"
#include "formats/afp_byte_order.h"
#include "support/expected.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

namespace AfpAnimation {

namespace {

bool SwapsBackgroundColour(std::span<const AfpByteOrder::Swap> swaps) {
    return std::ranges::any_of(swaps, [](const AfpByteOrder::Swap& swap) {
        const std::size_t field = AfpLayout::kBackgroundColourField;
        return swap.element_size == AfpLayout::kIntSize && swap.offset <= field &&
               field < swap.offset + (swap.count * swap.element_size) &&
               (field - swap.offset) % AfpLayout::kIntSize == 0;
    });
}

}

Support::Expected<Animation, std::string> ReadStored(std::span<const uint8_t> stored,
                                                     std::span<const uint8_t> script) {
    auto restored = AfpByteOrder::Restore(stored, script);
    if (!restored) return Support::Unexpected(restored.error());
    auto animation = Read(restored->data);
    if (!animation) return Support::Unexpected(animation.error());
    animation->stored_form = StoredForm{
        .strings_scrambled = restored->strings_scrambled,
        .background_colour_swapped = SwapsBackgroundColour(restored->swaps),
    };
    return animation;
}

Support::Expected<Stored, std::string> WriteStored(const Animation& animation) {
    auto native = Write(animation);
    if (!native) return Support::Unexpected(native.error());
    if (!native->swaps) return Support::Unexpected(native->swaps.error());
    auto script = AfpByteOrder::WriteScript(*native->swaps);
    if (!script) return Support::Unexpected(script.error());
    auto data =
        AfpByteOrder::Store(native->data, *native->swaps, animation.stored_form.strings_scrambled);
    if (!data) return Support::Unexpected(data.error());
    return Stored{.data = std::move(*data), .script = std::move(*script)};
}

}
