#pragma once

#include "document/authored.h"
#include "document/clip.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <string>

namespace Document {

[[nodiscard]] Support::Expected<void, std::string> TrimSpan(AfpAnimation::Animation& animation,
                                                            ClipId clip, uint16_t depth,
                                                            uint32_t frame, const Span& wanted);

[[nodiscard]] Support::Expected<void, std::string>
TrimOwnedSpan(AfpAnimation::Animation& animation, AuthoredDepth& authored, const Span& wanted);

[[nodiscard]] Support::Expected<void, std::string> TrimAuthored(AuthoredDepth& authored,
                                                                const Span& wanted);

[[nodiscard]] Support::Expected<void, std::string>
CarryUpdates(AfpAnimation::Container& clip, uint16_t depth, uint32_t first, uint32_t onto);

}
