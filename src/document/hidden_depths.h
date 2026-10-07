#pragma once

#include "document/clip.h"
#include "document/document.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Document {

struct DepthInClip {
    std::string animation;
    ClipId clip;
    uint16_t depth = 0;

    friend bool operator==(const DepthInClip&, const DepthInClip&) = default;
};

[[nodiscard]] Support::Expected<void, std::string>
HideDepths(AfpAnimation::Animation& animation, ClipId clip, const std::vector<uint16_t>& depths);

[[nodiscard]] Support::Expected<File, std::string>
ViewWithout(const File& file, const std::vector<DepthInClip>& hidden);

}
