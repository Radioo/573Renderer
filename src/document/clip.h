#pragma once

#include "document/outline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Document {

struct ClipId {
    std::optional<uint16_t> sprite;

    friend bool operator==(const ClipId&, const ClipId&) = default;
};

struct ClipSummary {
    ClipId id;
    std::string name;
    uint32_t frame_count = 0;
};

[[nodiscard]] std::vector<ClipSummary> Clips(const AfpAnimation::Animation& animation);

[[nodiscard]] std::string ClipLabel(const ClipSummary& clip);

[[nodiscard]] const AfpAnimation::Container* FindClip(const AfpAnimation::Animation& animation,
                                                      ClipId clip);

[[nodiscard]] AfpAnimation::Container* FindClip(AfpAnimation::Animation& animation, ClipId clip);

[[nodiscard]] Support::Expected<AfpAnimation::Container*, std::string>
RequireClip(AfpAnimation::Animation& animation, ClipId clip);

[[nodiscard]] std::string MissingClipMessage(ClipId clip);

[[nodiscard]] std::optional<AnimationDetails> DescribeClip(const AfpAnimation::Animation& animation,
                                                           ClipId clip);

}
