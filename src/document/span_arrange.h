#pragma once

#include "document/clip.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Document {

enum class Arrange : uint8_t { Forward, Backward, Front, Back };

struct DepthChange {
    uint16_t from = 0;
    uint16_t to = 0;

    friend bool operator==(const DepthChange&, const DepthChange&) = default;
};

[[nodiscard]] Support::Expected<std::vector<DepthChange>, std::string>
ArrangeSpan(AfpAnimation::Animation& animation, ClipId clip, uint16_t depth, uint32_t frame,
            Arrange how);

}
