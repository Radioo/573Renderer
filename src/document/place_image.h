#pragma once

#include "document/clip.h"
#include "document/document.h"
#include "support/expected.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Document {

struct DepthSpan {
    ClipId clip;
    uint16_t depth = 0;
    uint32_t first_frame = 0;
    uint32_t last_frame = 0;
};

[[nodiscard]] Support::Expected<uint16_t, std::string> PlaceImage(File& file,
                                                                  std::string_view animation_path,
                                                                  std::string_view image,
                                                                  const DepthSpan& span);

}
