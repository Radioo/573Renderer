#include "formats/afp_byte_order.h"

#include "formats/afp_layout.h"
#include "formats/little_endian.h"
#include "support/expected.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace AfpByteOrder {

namespace {

constexpr std::size_t kWordSize = 2;
constexpr unsigned kTypeShift = 13;
constexpr unsigned kLoopsShift = 7;
constexpr unsigned kLoopsMask = 0x3F;
constexpr unsigned kSkipMask = 0x7F;
constexpr unsigned kLargestType = 3;
constexpr std::size_t kMaxRun = 64;
constexpr std::size_t kMaxInlineSkip = 254;
constexpr std::size_t kSkipBlock = 256;
constexpr std::size_t kMaxSkipBlocks = 63;
constexpr uint32_t kMagic = 0x41503200;
constexpr std::size_t kMagicSize = 4;
constexpr std::array<uint8_t, 3> kOldMagic = {0x50, 0x46, 0x41};
constexpr std::array<uint8_t, 3> kOldMagicHighBits = {0xD0, 0xC6, 0xC1};
constexpr unsigned kScrambleFreeDataVersion = 1;
constexpr unsigned kScrambleBias = 0x80;

using LittleEndian::ReadU32;

uint32_t ByteSwapped(uint32_t value) {
    return ((value & 0xFFU) << 24U) | ((value & 0xFF00U) << 8U) | ((value >> 8U) & 0xFF00U) |
           (value >> 24U);
}

void AppendWord(std::vector<uint8_t>& out, unsigned word) {
    LittleEndian::AppendU16(out, static_cast<uint16_t>(word));
}

unsigned TypeOf(uint8_t element_size) {
    switch (element_size) {
    case 2:
        return 1;
    case 4:
        return 2;
    case 8:
        return 3;
    default:
        return 0;
    }
}

bool IsNativeOrder(std::span<const uint8_t> data) {
    return data.size() >= kMagicSize &&
           ((ReadU32(data, 0) ^ AfpLayout::kNativeMagicXor) & AfpLayout::kMagicMask) == 0;
}

bool IsOldFormat(std::span<const uint8_t> data) {
    if (data.size() < kOldMagic.size()) return false;
    const auto head = data.first(kOldMagic.size());
    return std::ranges::equal(head, kOldMagic) || std::ranges::equal(head, kOldMagicHighBits);
}

bool IsStoredOrder(std::span<const uint8_t> data) {
    return data.size() >= kMagicSize &&
           (ByteSwapped(ReadU32(data, 0)) & AfpLayout::kMagicMask) == kMagic;
}

Support::Expected<void, std::string> ApplySwaps(std::span<uint8_t> data,
                                                std::span<const Swap> swaps) {
    for (const Swap& swap : swaps) {
        const std::size_t size = swap.element_size;
        if (TypeOf(swap.element_size) == 0)
            return Support::Unexpected(std::format("byte swap of {}-byte elements", size));
        if (swap.count > data.size() / size || swap.offset > data.size() - (swap.count * size)) {
            return Support::Unexpected(
                std::format("byte swap at {} runs past the end of the data", swap.offset));
        }
        for (std::size_t i = 0; i < swap.count; i++) {
            std::ranges::reverse(data.subspan(swap.offset + (i * size), size));
        }
    }
    return {};
}

Support::Expected<std::span<uint8_t>, std::string> StringTableBytes(std::span<uint8_t> data) {
    if (data.size() < AfpLayout::kHeaderSize)
        return Support::Unexpected(std::string("animation header is truncated"));
    const std::size_t offset = ReadU32(data, AfpLayout::kStringTableField);
    const std::size_t size = ReadU32(data, AfpLayout::kStringTableSizeField);
    if (offset > data.size() || size > data.size() - offset)
        return Support::Unexpected(std::string("string table lies outside the animation"));
    return data.subspan(offset, size);
}

Support::Expected<std::vector<Swap>, std::string> MergeRuns(std::span<const Swap> swaps) {
    std::vector<Swap> runs;
    std::size_t end = 0;
    for (const Swap& swap : swaps) {
        if (TypeOf(swap.element_size) == 0 || swap.count == 0) {
            return Support::Unexpected(std::format("cannot express {} swaps of {}-byte elements",
                                                   swap.count, swap.element_size));
        }
        if (swap.offset < end) {
            return Support::Unexpected(
                std::format("byte swap at {} overlaps or precedes the previous one", swap.offset));
        }
        if (!runs.empty() && swap.offset == end && runs.back().element_size == swap.element_size) {
            runs.back().count += swap.count;
        } else {
            runs.push_back(swap);
        }
        end = swap.offset + (swap.count * swap.element_size);
    }
    return runs;
}

}

Support::Expected<std::vector<Swap>, std::string> ReadScript(std::span<const uint8_t> script) {
    if (script.size() % kWordSize != 0)
        return Support::Unexpected(std::string("byte order script has an odd length"));
    std::vector<Swap> swaps;
    std::size_t cursor = 0;
    for (std::size_t off = 0; off < script.size(); off += kWordSize) {
        const unsigned word = LittleEndian::ReadU16(script, off);
        if (word == 0) return swaps;
        cursor += (word & kSkipMask) * kWordSize;
        const unsigned type = word >> kTypeShift;
        const std::size_t loops = (word >> kLoopsShift) & kLoopsMask;
        if (type == 0) {
            cursor += loops * kSkipBlock;
            continue;
        }
        if (type > kLargestType)
            return Support::Unexpected(std::format("unknown byte order data type({})", type));
        const auto size = static_cast<uint8_t>(1U << type);
        swaps.push_back(Swap{.offset = cursor, .element_size = size, .count = loops + 1});
        cursor += size * (loops + 1);
    }
    return Support::Unexpected(std::string("byte order script has no terminator"));
}

Support::Expected<std::vector<uint8_t>, std::string> WriteScript(std::span<const Swap> swaps) {
    auto runs = MergeRuns(swaps);
    if (!runs) return Support::Unexpected(runs.error());
    std::vector<uint8_t> out;
    std::size_t cursor = 0;
    for (const Swap& run : *runs) {
        std::size_t offset = run.offset;
        std::size_t remaining = run.count;
        while (remaining > 0) {
            const std::size_t chunk = std::min(remaining, kMaxRun);
            std::size_t gap = offset - cursor;
            if (gap % kWordSize != 0) {
                return Support::Unexpected(
                    std::format("byte swap at {} is not word aligned", offset));
            }
            while (gap > kMaxInlineSkip) {
                const std::size_t blocks = std::min(gap / kSkipBlock, kMaxSkipBlocks);
                std::size_t rest = gap - (blocks * kSkipBlock);
                if (rest > kMaxInlineSkip) rest = 0;
                AppendWord(out,
                           static_cast<unsigned>((blocks << kLoopsShift) | (rest / kWordSize)));
                gap -= (blocks * kSkipBlock) + rest;
            }
            AppendWord(out,
                       (TypeOf(run.element_size) << kTypeShift) |
                           static_cast<unsigned>(((chunk - 1) << kLoopsShift) | (gap / kWordSize)));
            offset += chunk * run.element_size;
            remaining -= chunk;
            cursor = offset;
        }
    }
    AppendWord(out, 0);
    return out;
}

Support::Expected<Restored, std::string> Restore(std::span<const uint8_t> stored,
                                                 std::span<const uint8_t> script) {
    Restored restored{.data = {stored.begin(), stored.end()}, .swaps = {}};
    if (IsOldFormat(restored.data)) return restored;
    if (!IsNativeOrder(restored.data)) {
        if (!IsStoredOrder(restored.data))
            return Support::Unexpected(std::string("this is not afp data"));
        auto swaps = ReadScript(script);
        if (!swaps) return Support::Unexpected(swaps.error());
        restored.swaps = std::move(*swaps);
        auto applied = ApplySwaps(restored.data, restored.swaps);
        if (!applied) return Support::Unexpected(applied.error());
        if (!IsNativeOrder(restored.data))
            return Support::Unexpected(std::string("byte order script does not restore the magic"));
    }
    auto table = StringTableBytes(restored.data);
    if (!table) return Support::Unexpected(table.error());
    if (LittleEndian::ReadU16(restored.data, AfpLayout::kDataVersionField) ==
            kScrambleFreeDataVersion ||
        table->empty() || (*table)[0] == 0)
        return restored;
    if ((*table)[0] != AfpLayout::kScrambledStringsMarker)
        return Support::Unexpected(std::string("afp data string buffer unusual"));
    for (std::size_t i = 0; i < table->size(); i++) {
        (*table)[i] = static_cast<uint8_t>((*table)[i] - kScrambleBias - i);
    }
    restored.strings_scrambled = true;
    return restored;
}

Support::Expected<std::vector<uint8_t>, std::string>
Store(std::span<const uint8_t> native, std::span<const Swap> swaps, bool scramble_strings) {
    if (!IsNativeOrder(native))
        return Support::Unexpected(std::string("animation is not in native byte order"));
    std::vector<uint8_t> data(native.begin(), native.end());
    if (scramble_strings) {
        auto table = StringTableBytes(data);
        if (!table) return Support::Unexpected(table.error());
        if (LittleEndian::ReadU16(data, AfpLayout::kDataVersionField) == kScrambleFreeDataVersion) {
            return Support::Unexpected(
                std::string("data version 1 string tables are never scrambled"));
        }
        if (!table->empty() && (*table)[0] != 0)
            return Support::Unexpected(std::string("string table is not plain"));
        for (std::size_t i = 0; i < table->size(); i++) {
            (*table)[i] = static_cast<uint8_t>((*table)[i] + kScrambleBias + i);
        }
    }
    auto applied = ApplySwaps(data, swaps);
    if (!applied) return Support::Unexpected(applied.error());
    return data;
}

}
