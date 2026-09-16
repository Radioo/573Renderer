#include "document/timeline.h"

#include "formats/afp_animation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <variant>
#include <vector>

namespace Document {

namespace {

constexpr uint32_t kUpdateExisting = 0x1;

void Close(std::map<uint16_t, DepthRow>& rows, std::map<uint16_t, uint32_t>& open, uint16_t depth,
           uint32_t last_frame) {
    const auto found = open.find(depth);
    if (found == open.end()) return;
    if (found->second > last_frame) {
        open.erase(found);
        return;
    }
    rows[depth].depth = depth;
    rows[depth].spans.push_back(Span{.first_frame = found->second, .last_frame = last_frame});
    open.erase(found);
}

}

std::vector<DepthRow> DepthRows(const AfpAnimation::Container& clip) {
    std::map<uint16_t, DepthRow> rows;
    std::map<uint16_t, uint32_t> open;
    for (std::size_t index = 0; index < clip.frames.size(); index++) {
        const AfpAnimation::Frame& frame = clip.frames[index];
        const auto number = static_cast<uint32_t>(index);
        for (uint32_t tag = 0; tag < frame.tag_count; tag++) {
            const std::size_t position = frame.first_tag + tag;
            if (position >= clip.tags.size()) break;
            const auto& body = clip.tags[position].body;
            if (const auto* placement = std::get_if<AfpAnimation::Placement>(&body)) {
                if ((placement->flags & kUpdateExisting) != 0) continue;
                if (number > 0) Close(rows, open, placement->depth, number - 1);
                open.emplace(placement->depth, number);
                rows[placement->depth].depth = placement->depth;
            } else if (const auto* remove = std::get_if<AfpAnimation::Remove>(&body)) {
                Close(rows, open, remove->depth, number > 0 ? number - 1 : 0);
            }
        }
    }
    const auto last = clip.frames.empty() ? 0 : static_cast<uint32_t>(clip.frames.size() - 1);
    while (!open.empty())
        Close(rows, open, open.begin()->first, last);
    std::vector<DepthRow> out;
    out.reserve(rows.size());
    for (auto& [depth, row] : rows) {
        std::ranges::sort(row.spans, {}, &Span::first_frame);
        out.push_back(std::move(row));
    }
    return out;
}

}
