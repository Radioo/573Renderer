#pragma once

#include "document/authored.h"
#include "document/clip.h"
#include "document/stage_bounds.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <string>

namespace Document {

struct StageOffset {
    double x = 0;
    double y = 0;
};

[[nodiscard]] Support::Expected<void, std::string>
MoveBakedDepth(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth, uint32_t frame,
               StageOffset offset);

[[nodiscard]] Support::Expected<void, std::string> MoveOwnedDepth(AuthoredDepth& authored,
                                                                  const BakedDepth& baked,
                                                                  uint32_t frame,
                                                                  StageOffset offset);

[[nodiscard]] Support::Expected<void, std::string>
PlaceAtPoint(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth, uint16_t character,
             uint32_t first_frame, uint32_t last_frame, StageOffset point);

[[nodiscard]] Support::Expected<void, std::string>
ReshapeBakedDepth(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth, uint32_t frame,
                  const Reshape& reshape);

[[nodiscard]] Support::Expected<void, std::string> ReshapeOwnedDepth(AuthoredDepth& authored,
                                                                     const BakedDepth& baked,
                                                                     uint32_t frame,
                                                                     const Reshape& reshape);

}
