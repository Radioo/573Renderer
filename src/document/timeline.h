#pragma once

#include "formats/afp_animation.h"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace Document {

struct Span {
    uint32_t first_frame = 0;
    uint32_t last_frame = 0;

    friend bool operator==(const Span&, const Span&) = default;
};

struct DepthRow {
    uint16_t depth = 0;
    std::vector<Span> spans;
    std::map<uint32_t, uint16_t> shows;
};

enum class Direction : uint8_t { Back, Forward };

[[nodiscard]] std::vector<DepthRow> DepthRows(const AfpAnimation::Container& clip);

[[nodiscard]] std::optional<uint16_t> UnusedDepth(const AfpAnimation::Container& clip);

[[nodiscard]] std::vector<uint32_t> DepthMarks(const AfpAnimation::Container& clip, uint16_t depth);

[[nodiscard]] std::optional<uint32_t> NextMark(const std::vector<uint32_t>& marks, uint32_t frame,
                                               Direction direction);

}
