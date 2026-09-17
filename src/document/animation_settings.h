#pragma once

#include "document/outline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <string>
#include <string_view>
#include <vector>

namespace Document {

[[nodiscard]] std::vector<Field> AnimationSettingFields(const AfpAnimation::Animation& animation);

[[nodiscard]] Support::Expected<void, std::string>
SetAnimationSetting(AfpAnimation::Animation& animation, std::string_view name,
                    std::string_view value);

}
