#pragma once

#include "formats/afp_animation.h"

#include <cstddef>
#include <cstdint>

namespace Document {

void InsertTag(AfpAnimation::Container& clip, uint32_t frame, AfpAnimation::Tag tag);

void EraseTag(AfpAnimation::Container& clip, std::size_t at);

}
