#pragma once

#include "document/clip.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Document {

struct SpanShift {
    uint16_t depth = 0;
    uint32_t frame = 0;
    int64_t by = 0;

    friend bool operator==(const SpanShift&, const SpanShift&) = default;
};

[[nodiscard]] Support::Expected<std::vector<SpanShift>, std::string>
SequenceSpans(AfpAnimation::Animation& animation, ClipId clip, std::vector<uint16_t> depths,
              uint32_t frame);

}
