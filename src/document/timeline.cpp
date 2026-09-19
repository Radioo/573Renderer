#include "document/timeline.h"

#include "formats/afp_animation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <optional>
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

void Walk(const AfpAnimation::Tag& tag, uint32_t number, std::map<uint16_t, DepthRow>& rows,
          std::map<uint16_t, uint32_t>& open) {
    if (const auto* placement = std::get_if<AfpAnimation::Placement>(&tag.body)) {
        if ((placement->flags & kUpdateExisting) != 0) return;
        if (number > 0) Close(rows, open, placement->depth, number - 1);
        open.emplace(placement->depth, number);
        DepthRow& row = rows[placement->depth];
        row.depth = placement->depth;
        if (placement->character) row.shows[number] = *placement->character;
    } else if (const auto* remove = std::get_if<AfpAnimation::Remove>(&tag.body)) {
        Close(rows, open, remove->depth, number > 0 ? number - 1 : 0);
    }
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
            Walk(clip.tags[position], number, rows, open);
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

std::vector<uint32_t> DepthMarks(const AfpAnimation::Container& clip, uint16_t depth) {
    std::vector<uint32_t> marks;
    for (std::size_t index = 0; index < clip.frames.size(); index++) {
        const AfpAnimation::Frame& frame = clip.frames[index];
        for (uint32_t tag = 0; tag < frame.tag_count; tag++) {
            const std::size_t position = frame.first_tag + tag;
            if (position >= clip.tags.size()) break;
            const auto& body = clip.tags[position].body;
            const auto* placement = std::get_if<AfpAnimation::Placement>(&body);
            const auto* remove = std::get_if<AfpAnimation::Remove>(&body);
            const bool touches = (placement != nullptr && placement->depth == depth) ||
                                 (remove != nullptr && remove->depth == depth);
            if (!touches) continue;
            marks.push_back(static_cast<uint32_t>(index));
            break;
        }
    }
    return marks;
}

std::optional<uint32_t> NextMark(const std::vector<uint32_t>& marks, uint32_t frame,
                                 Direction direction) {
    if (direction == Direction::Forward) {
        const auto after = std::ranges::upper_bound(marks, frame);
        if (after == marks.end()) return std::nullopt;
        return *after;
    }
    const auto before = std::ranges::lower_bound(marks, frame);
    if (before == marks.begin()) return std::nullopt;
    return *std::prev(before);
}

}
