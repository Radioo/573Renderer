#pragma once

#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Document {

[[nodiscard]] std::vector<int64_t> FilterNumbers(const std::vector<AfpAnimation::Filter>& filters);

[[nodiscard]] Support::Expected<std::vector<AfpAnimation::Filter>, std::string>
FiltersFrom(std::span<const int64_t> numbers);

}
