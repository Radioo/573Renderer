#pragma once

#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

[[nodiscard]] std::span<const std::string_view> AnimatableProperties();

[[nodiscard]] bool PropertyIsAnimatable(std::string_view name);

[[nodiscard]] bool PropertyIsStepped(std::string_view name);

[[nodiscard]] std::optional<std::vector<int64_t>>
ReadProperty(const AfpAnimation::Placement& placement, std::string_view name);

[[nodiscard]] Support::Expected<void, std::string> WriteProperty(AfpAnimation::Placement& placement,
                                                                 std::string_view name,
                                                                 std::span<const int64_t> value);

void ClearAnimatableProperties(AfpAnimation::Placement& placement);

[[nodiscard]] std::vector<std::string> UnanimatableParts(const AfpAnimation::Placement& placement);

}
