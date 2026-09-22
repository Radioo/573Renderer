#pragma once

#include "formats/afp_animation.h"

#include <cstdint>
#include <vector>

namespace Document {

struct FrameNote {
    uint32_t frame = 0;
    bool script = false;
    bool camera = false;
};

[[nodiscard]] std::vector<FrameNote> FrameNotes(const AfpAnimation::Container& clip);

}
