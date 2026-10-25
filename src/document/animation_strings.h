#pragma once

#include "formats/afp_animation.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Document {

[[nodiscard]] AfpAnimation::StringId InternString(AfpAnimation::Animation& animation,
                                                  std::string_view text);

void CompactStrings(AfpAnimation::Animation& animation);

[[nodiscard]] std::string FoldedName(std::string_view text);

void InsertExport(AfpAnimation::Animation& animation, uint16_t tag, std::string_view name);

void CarryStrings(AfpAnimation::Tag& tag, const AfpAnimation::Animation& from,
                  AfpAnimation::Animation& to);

[[nodiscard]] std::string StringText(const AfpAnimation::Animation& animation,
                                     AfpAnimation::StringId id);

}
