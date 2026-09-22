#pragma once

#include "document/clip.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <string>

namespace Document {

[[nodiscard]] Support::Expected<void, std::string>
AddDepth(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth, uint16_t character,
         uint32_t first_frame, uint32_t last_frame);

[[nodiscard]] Support::Expected<void, std::string>
RemoveDepth(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth, uint32_t frame);

[[nodiscard]] Support::Expected<void, std::string> InsertFrame(AfpAnimation::Animation& animation,
                                                               ClipId clip, uint32_t at);

[[nodiscard]] Support::Expected<void, std::string> RemoveFrame(AfpAnimation::Animation& animation,
                                                               ClipId clip, uint32_t at);

}
