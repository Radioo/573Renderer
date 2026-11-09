#pragma once

#include "document/clip.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Document {

[[nodiscard]] Support::Expected<void, std::string>
AddLabel(AfpAnimation::Animation& animation, ClipId clip, std::string_view name, uint32_t frame);

[[nodiscard]] Support::Expected<void, std::string> RenameLabel(AfpAnimation::Animation& animation,
                                                               ClipId clip, std::string_view name,
                                                               std::string_view renamed);

[[nodiscard]] Support::Expected<void, std::string>
MoveLabel(AfpAnimation::Animation& animation, ClipId clip, std::string_view name, uint32_t frame);

[[nodiscard]] Support::Expected<void, std::string> RemoveLabel(AfpAnimation::Animation& animation,
                                                               ClipId clip, std::string_view name);

}
