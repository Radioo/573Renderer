#pragma once

#include "support/expected.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

enum class Ease : uint8_t { Hold, Linear, Bezier };

struct Bezier {
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 1.0;
    double y2 = 1.0;

    friend bool operator==(const Bezier&, const Bezier&) = default;
};

struct Keyframe {
    uint32_t frame = 0;
    std::vector<int64_t> value;
    Ease ease = Ease::Linear;
    Bezier bezier;

    friend bool operator==(const Keyframe&, const Keyframe&) = default;
};

struct Track {
    std::string property;
    std::vector<Keyframe> keys;

    friend bool operator==(const Track&, const Track&) = default;
};

[[nodiscard]] std::string_view EaseName(Ease ease);

[[nodiscard]] std::optional<Ease> EaseFor(std::string_view name);

[[nodiscard]] Support::Expected<void, std::string> CheckTrack(const Track& track);

[[nodiscard]] std::vector<int64_t> SampleTrack(const Track& track, uint32_t frame);

[[nodiscard]] Support::Expected<void, std::string> AddKeyframe(Track& track, const Keyframe& key);

[[nodiscard]] Support::Expected<void, std::string>
SetKeyframeValue(Track& track, uint32_t frame, const std::vector<int64_t>& value);

[[nodiscard]] Support::Expected<void, std::string> SetKeyframeEase(Track& track, uint32_t frame,
                                                                   Ease ease, Bezier bezier);

[[nodiscard]] Support::Expected<void, std::string> RetimeKeyframe(Track& track, uint32_t from,
                                                                  uint32_t to);

[[nodiscard]] Support::Expected<void, std::string> RemoveKeyframe(Track& track, uint32_t frame);

}
