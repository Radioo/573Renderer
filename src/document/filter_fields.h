#pragma once

#include "document/outline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <string>
#include <string_view>
#include <vector>

namespace Document {

[[nodiscard]] std::vector<Field> FilterFields(const std::vector<AfpAnimation::Filter>& filters);

[[nodiscard]] bool FilterFieldIsEditable(std::string_view name);

[[nodiscard]] Support::Expected<void, std::string>
SetFilterField(std::vector<AfpAnimation::Filter>& filters, std::string_view name,
               std::string_view value);

}
