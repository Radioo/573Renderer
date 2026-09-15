#include "formats/ifs_layout.h"

#include "formats/binary_xml.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Ifs::Detail {

namespace {

constexpr uint32_t kFileAlignment = 4;
constexpr uint32_t kEndAlignment = 16;
constexpr std::size_t kSixBitNodeBytes = 52;
constexpr std::size_t kLongNameNodeBytes = 56;
constexpr std::size_t kTreeBaseBytes = 630;
constexpr std::size_t kMinLongNameBytes = 8;
constexpr std::size_t kTreeSizeSlack = 8;
constexpr std::size_t kSmallValueBytes = 4;
constexpr std::string_view kEscapeSeconds = "ABCDEFGH_0123456789";

struct Gap {
    uint32_t start = 0;
    uint32_t end = 0;
};

struct TreeTally {
    std::size_t nodes = 0;
    std::size_t data = 0;
    std::size_t names = 0;
};

void Tally(const BinaryXml::Node& node, TreeTally& tally) {
    tally.nodes++;
    tally.names += (std::max(node.name.size(), kMinLongNameBytes) + 3U) & ~std::size_t{3};
    const std::size_t m = node.value.size();
    if (m > kSmallValueBytes) {
        tally.data += node.type == kBinType ? ((m + 1U) & ~std::size_t{1}) : ((m + 3U) & ~std::size_t{3});
    }
    for (const BinaryXml::Node& child : node.children)
        Tally(child, tally);
}

}

Layout PackLargestFirst(std::span<const uint32_t> sizes) {
    Layout layout;
    layout.offsets.assign(sizes.size(), 0);
    std::vector<std::size_t> order(sizes.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::stable_sort(order, [&](std::size_t a, std::size_t b) { return sizes[a] > sizes[b]; });

    std::vector<Gap> gaps;
    uint32_t end = 0;
    for (const std::size_t index : order) {
        const uint32_t size = sizes[index];
        const auto gap = std::ranges::find_if(gaps, [&](const Gap& g) {
            return AlignTo(g.start, kFileAlignment) + size <= g.end;
        });
        if (gap != gaps.end()) {
            const uint32_t start = AlignTo(gap->start, kFileAlignment);
            layout.offsets[index] = start;
            gap->start = start + size;
            continue;
        }
        layout.offsets[index] = end;
        const uint32_t file_end = end + size;
        end = AlignTo(file_end, kEndAlignment);
        if (end > file_end) gaps.push_back({.start = file_end, .end = end});
    }
    layout.data_size = end;
    return layout;
}

uint32_t TreeSize(const BinaryXml::Document& manifest) {
    TreeTally tally;
    Tally(manifest.root, tally);
    std::size_t size = 0;
    if (manifest.signature == BinaryXml::kByteNames) {
        size = (kLongNameNodeBytes * tally.nodes) + tally.data + kTreeBaseBytes + tally.names;
    } else {
        size = (kSixBitNodeBytes * tally.nodes) + tally.data + kTreeBaseBytes;
    }
    return static_cast<uint32_t>((size + kTreeSizeSlack) & ~std::size_t{7});
}

bool IsSpecialName(const std::string& name) {
    return name.size() >= 2 && name[0] == '_' && kEscapeSeconds.find(name[1]) == std::string_view::npos;
}

}
