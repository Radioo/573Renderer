#pragma once

#include "document/clip.h"
#include "document/timeline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Document {

struct CopiedSpan {
    std::string animation;
    uint32_t length = 0;
    std::vector<std::pair<uint32_t, AfpAnimation::Placement>> placements;
};

[[nodiscard]] Support::Expected<CopiedSpan, std::string>
CopySpan(const AfpAnimation::Animation& animation, std::string_view animation_path, ClipId clip,
         uint16_t depth, uint32_t frame);

[[nodiscard]] Support::Expected<Span, std::string> PasteSpan(AfpAnimation::Animation& animation,
                                                             std::string_view animation_path,
                                                             ClipId clip, const CopiedSpan& copied,
                                                             uint16_t depth, uint32_t first_frame);

}
