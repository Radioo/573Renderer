#pragma once

#include "document/authored.h"
#include "document/clip.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <optional>
#include <string>

namespace Document {

[[nodiscard]] std::optional<Span> SpanOfDepth(const AfpAnimation::Container& clip, uint16_t depth,
                                              uint32_t frame);

[[nodiscard]] std::optional<Span> NearestSpan(const AfpAnimation::Container& clip, uint16_t depth,
                                              uint32_t frame);

[[nodiscard]] Support::Expected<Span, std::string> MoveSpan(AfpAnimation::Animation& animation,
                                                            ClipId clip, uint16_t depth,
                                                            uint32_t frame, int64_t by);

[[nodiscard]] Support::Expected<void, std::string>
ChangeSpanDepth(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth, uint32_t frame,
                uint16_t to);

[[nodiscard]] std::optional<uint16_t> FreeDepthAbove(const AfpAnimation::Container& clip,
                                                     uint16_t depth, uint32_t frame);

[[nodiscard]] Support::Expected<void, std::string> DuplicateSpan(AfpAnimation::Animation& animation,
                                                                 ClipId clip, uint16_t depth,
                                                                 uint32_t frame, uint16_t to);

[[nodiscard]] Support::Expected<void, std::string> ShiftAuthored(AuthoredDepth& authored,
                                                                 int64_t by);

}
