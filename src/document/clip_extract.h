#pragma once

#include "document/clip.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstddef>
#include <string>

namespace Document {

[[nodiscard]] Support::Expected<std::size_t, std::string>
ExtractFrames(AfpAnimation::Animation& animation, ClipId clip, const Span& cut);

}
