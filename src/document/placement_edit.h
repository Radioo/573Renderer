#pragma once

#include "document/outline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

[[nodiscard]] std::optional<std::size_t> LivePlacementTag(const AfpAnimation::Container& clip,
                                                          uint16_t depth, uint32_t frame);

[[nodiscard]] std::vector<Field> PlacementFields(const AfpAnimation::Animation& animation,
                                                 const AfpAnimation::Placement& placement);

[[nodiscard]] bool PlacementFieldIsEditable(std::string_view name);

[[nodiscard]] Support::Expected<void, std::string>
SetPlacementField(AfpAnimation::Animation& animation, AfpAnimation::Placement& placement,
                  std::string_view name, std::string_view value);

}
