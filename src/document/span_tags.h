#pragma once

#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Document {

[[nodiscard]] bool IsRemoveOf(const AfpAnimation::Tag& tag, uint16_t depth);

[[nodiscard]] bool IsPlacementOf(const AfpAnimation::Tag& tag, uint16_t depth);

[[nodiscard]] std::vector<std::size_t> SpanTags(const AfpAnimation::Container& clip, uint16_t depth,
                                                const Span& span);

void InsertTagFirst(AfpAnimation::Container& clip, uint32_t frame, AfpAnimation::Tag tag);

[[nodiscard]] bool TouchedBetween(const AfpAnimation::Container& clip, uint16_t depth,
                                  uint32_t first, uint32_t last);

[[nodiscard]] Support::Expected<void, std::string>
CheckFree(const AfpAnimation::Container& clip, uint16_t depth, uint32_t first, uint32_t last);

}
