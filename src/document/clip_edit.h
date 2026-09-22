#pragma once

#include "document/clip.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Document {

[[nodiscard]] Support::Expected<void, std::string>
EditPlacementField(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth, uint32_t frame,
                   std::string_view field, std::string_view value);

[[nodiscard]] Support::Expected<void, std::string>
EditCallArgument(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth, uint32_t frame,
                 std::size_t index, std::string_view value);

[[nodiscard]] std::optional<std::size_t> FrameScriptTag(const AfpAnimation::Container& clip,
                                                        uint32_t frame);

[[nodiscard]] Support::Expected<void, std::string>
EditFrameCallArgument(AfpAnimation::Animation& animation, ClipId clip, uint32_t frame,
                      std::size_t index, std::string_view value);

[[nodiscard]] Support::Expected<void, std::string>
WriteFrameScript(AfpAnimation::Animation& animation, ClipId clip, uint32_t frame,
                 std::string_view source);

[[nodiscard]] Support::Expected<void, std::string>
WritePlacementScript(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth,
                     uint32_t frame, std::string_view source);

[[nodiscard]] Support::Expected<void, std::string>
EditCameraField(AfpAnimation::Animation& animation, ClipId clip, uint32_t frame,
                std::string_view field, std::string_view value);

}
