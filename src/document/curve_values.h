#pragma once

#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Document {

[[nodiscard]] std::vector<int64_t> CurveNumbers(const std::vector<AfpAnimation::Curve>& curves);

[[nodiscard]] Support::Expected<std::vector<AfpAnimation::Curve>, std::string>
CurvesFrom(std::span<const int64_t> numbers);

[[nodiscard]] Support::Expected<void, std::string>
CheckCurvesFit(const std::vector<AfpAnimation::Curve>& first,
               const std::vector<AfpAnimation::Curve>& later);

}
