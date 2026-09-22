#pragma once

#include "document/clip.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <optional>
#include <string>

namespace Document {

[[nodiscard]] std::optional<uint16_t> CharacterOn(const AfpAnimation::Container& clip,
                                                  uint16_t depth, const Span& span, uint32_t frame);

[[nodiscard]] bool IsStillCharacter(const AfpAnimation::Animation& animation, uint16_t character);

[[nodiscard]] Support::Expected<void, std::string>
SplitSpan(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth, uint32_t frame);

[[nodiscard]] Support::Expected<void, std::string>
SplitSpanRestarting(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth,
                    const Span& span, uint32_t frame);

}
