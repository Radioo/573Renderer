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

enum class NewFilter : uint8_t { ColourMatrix, Hsv };

[[nodiscard]] std::vector<Field> FilterFields(const std::vector<AfpAnimation::Filter>& filters);

[[nodiscard]] bool FilterFieldIsEditable(std::string_view name);

[[nodiscard]] Support::Expected<void, std::string>
SetFilterField(std::vector<AfpAnimation::Filter>& filters, std::string_view name,
               std::string_view value);

[[nodiscard]] std::optional<std::size_t> FilterNumberOf(std::string_view name);

void AddFilter(std::vector<AfpAnimation::Filter>& filters, NewFilter kind);

[[nodiscard]] Support::Expected<void, std::string>
RemoveFilter(std::vector<AfpAnimation::Filter>& filters, std::string_view name);

}
