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

[[nodiscard]] std::optional<std::size_t> CameraTag(const AfpAnimation::Container& clip,
                                                   uint32_t frame);

[[nodiscard]] std::vector<Field> CameraFields(const AfpAnimation::Camera& camera);

[[nodiscard]] bool CameraFieldIsEditable(std::string_view name);

[[nodiscard]] Support::Expected<void, std::string>
SetCameraField(AfpAnimation::Camera& camera, std::string_view name, std::string_view value);

[[nodiscard]] Support::Expected<void, std::string> AddCamera(AfpAnimation::Animation& animation,
                                                             uint32_t frame, uint16_t id);

[[nodiscard]] Support::Expected<void, std::string> RemoveCamera(AfpAnimation::Animation& animation,
                                                                uint32_t frame);

}
