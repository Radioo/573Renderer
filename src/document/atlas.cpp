#include "document/atlas.h"

#include "support/expected.h"

#include <stb_rect_pack.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

void StableRectSort(void* base, std::size_t count, std::size_t size,
                    int (*compare)(const void*, const void*)) {
    (void)size;
    auto* rects = static_cast<stbrp_rect*>(base);
    std::stable_sort(rects, rects + count, [compare](const stbrp_rect& a, const stbrp_rect& b) {
        return compare(&a, &b) < 0;
    });
}

}

#define STBRP_SORT StableRectSort
#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

namespace Document {

namespace {

constexpr uint32_t kSmallestAtlas = 64;
constexpr uint32_t kLargestAtlas = 4096;

std::vector<AtlasCell> InOrder(std::span<const AtlasCell> cells) {
    std::vector<AtlasCell> sorted(cells.begin(), cells.end());
    std::ranges::sort(sorted, {}, &AtlasCell::name);
    return sorted;
}

std::vector<std::pair<uint32_t, uint32_t>> CandidateSizes() {
    std::vector<std::pair<uint32_t, uint32_t>> sizes;
    for (uint32_t width = kSmallestAtlas; width <= kLargestAtlas; width *= 2) {
        for (uint32_t height = kSmallestAtlas; height <= kLargestAtlas; height *= 2)
            sizes.emplace_back(width, height);
    }
    std::ranges::sort(sizes, {}, [](const std::pair<uint32_t, uint32_t>& size) {
        return std::pair{static_cast<uint64_t>(size.first) * size.second, size.first};
    });
    return sizes;
}

bool Fits(const std::vector<AtlasCell>& cells, uint32_t width, uint32_t height,
          std::vector<AtlasPlacement>& placed) {
    std::vector<stbrp_rect> rects;
    rects.reserve(cells.size());
    for (std::size_t i = 0; i < cells.size(); i++) {
        rects.push_back(stbrp_rect{.id = static_cast<int>(i),
                                   .w = static_cast<stbrp_coord>(cells[i].width),
                                   .h = static_cast<stbrp_coord>(cells[i].height),
                                   .x = 0,
                                   .y = 0,
                                   .was_packed = 0});
    }
    std::vector<stbrp_node> nodes(width);
    stbrp_context context{};
    stbrp_init_target(&context, static_cast<int>(width), static_cast<int>(height), nodes.data(),
                      static_cast<int>(nodes.size()));
    if (stbrp_pack_rects(&context, rects.data(), static_cast<int>(rects.size())) == 0) return false;

    placed.assign(cells.size(), AtlasPlacement{});
    for (const stbrp_rect& rect : rects) {
        const auto at = static_cast<std::size_t>(rect.id);
        placed[at] = AtlasPlacement{.name = cells[at].name,
                                    .x = static_cast<uint32_t>(rect.x),
                                    .y = static_cast<uint32_t>(rect.y),
                                    .width = cells[at].width,
                                    .height = cells[at].height};
    }
    return true;
}

}

Support::Expected<Atlas, std::string> PackAtlas(std::span<const AtlasCell> cells) {
    if (cells.empty()) return Support::Unexpected(std::string("an atlas holds at least one image"));
    const std::vector<AtlasCell> sorted = InOrder(cells);
    for (std::size_t i = 0; i < sorted.size(); i++) {
        if (sorted[i].width == 0 || sorted[i].height == 0)
            return Support::Unexpected(sorted[i].name + " has no size");
        if (sorted[i].width > kLargestAtlas || sorted[i].height > kLargestAtlas) {
            return Support::Unexpected(sorted[i].name + " is larger than an atlas can be, " +
                                       std::to_string(kLargestAtlas) + " pixels on a side");
        }
        if (i > 0 && sorted[i].name == sorted[i - 1].name)
            return Support::Unexpected(sorted[i].name + " is in the atlas twice");
    }

    std::vector<AtlasPlacement> placed;
    for (const auto& [width, height] : CandidateSizes()) {
        if (Fits(sorted, width, height, placed))
            return Atlas{.width = width, .height = height, .images = std::move(placed)};
    }
    return Support::Unexpected(std::string("these images do not fit an atlas of ") +
                               std::to_string(kLargestAtlas) + " by " +
                               std::to_string(kLargestAtlas) + " pixels");
}

}
