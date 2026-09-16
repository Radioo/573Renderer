#pragma once

#include "formats/afp_animation.h"

#include <string>
#include <string_view>

namespace Document {

[[nodiscard]] AfpAnimation::StringId InternString(AfpAnimation::Animation& animation,
                                                  std::string_view text);

void CompactStrings(AfpAnimation::Animation& animation);

[[nodiscard]] std::string StringText(const AfpAnimation::Animation& animation,
                                     AfpAnimation::StringId id);

}
