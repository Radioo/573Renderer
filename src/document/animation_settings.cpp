#include "document/animation_settings.h"

#include "document/field_values.h"
#include "document/outline.h"
#include "formats/afp_animation.h"
#include "support/expected.h"

#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace Document {

namespace {

constexpr std::string_view kStageSize = "Stage size";
constexpr std::string_view kFrameRate = "Frame rate";
constexpr std::string_view kBackgroundColour = "Background colour";
constexpr std::string_view kUseBackground = "Use background colour";
constexpr std::string_view kOn = "on";
constexpr std::string_view kOff = "off";
constexpr uint32_t kColourFlag = 0x1;
constexpr uint32_t kFixedRateFlag = 0x2;
constexpr double kFixedRateScale = 1024.0;
constexpr std::size_t kMinX = 0;
constexpr std::size_t kMaxX = 1;
constexpr std::size_t kMinY = 2;
constexpr std::size_t kMaxY = 3;

bool FixedRate(const AfpAnimation::Animation& animation) {
    return (animation.flags & kFixedRateFlag) != 0;
}

std::string RateText(const AfpAnimation::Animation& animation) {
    if (!FixedRate(animation)) return std::format("{}", std::bit_cast<float>(animation.fps));
    std::string text = std::format(
        "{:.4f}", static_cast<double>(std::bit_cast<int32_t>(animation.fps)) / kFixedRateScale);
    while (text.back() == '0')
        text.pop_back();
    if (text.back() == '.') text.pop_back();
    return text;
}

Support::Expected<void, std::string> SetStageSize(AfpAnimation::Animation& animation,
                                                  std::string_view value) {
    auto numbers = Numbers(value, 2);
    if (!numbers) return Support::Unexpected(numbers.error());
    const int64_t width = (*numbers)[0];
    const int64_t height = (*numbers)[1];
    const int64_t highest = std::numeric_limits<uint16_t>::max();
    if (width <= 0 || height <= 0)
        return Support::Unexpected(std::string("a stage is at least one pixel on each side"));
    if (animation.rect[kMinX] + width > highest || animation.rect[kMinY] + height > highest)
        return Support::Unexpected(std::string("the stage does not fit the header's rect"));
    animation.rect[kMaxX] = static_cast<uint16_t>(animation.rect[kMinX] + width);
    animation.rect[kMaxY] = static_cast<uint16_t>(animation.rect[kMinY] + height);
    return {};
}

Support::Expected<void, std::string> SetFrameRate(AfpAnimation::Animation& animation,
                                                  std::string_view value) {
    double rate = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), rate);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
        !std::isfinite(rate) || rate <= 0) {
        return Support::Unexpected(std::string(value) + " is not a frame rate");
    }
    if (!FixedRate(animation)) {
        if (rate > std::numeric_limits<float>::max())
            return Support::Unexpected(std::string(value) + " is not a frame rate");
        animation.fps = std::bit_cast<uint32_t>(static_cast<float>(rate));
        return {};
    }
    const double stored = std::round(rate * kFixedRateScale);
    if (stored < 1 || stored > std::numeric_limits<int32_t>::max()) {
        return Support::Unexpected(std::string(value) +
                                   " does not fit the header's fixed point rate");
    }
    animation.fps = std::bit_cast<uint32_t>(static_cast<int32_t>(stored));
    return {};
}

Support::Expected<void, std::string> SetBackgroundColour(AfpAnimation::Animation& animation,
                                                         std::string_view value) {
    auto numbers = Numbers(value, animation.background_colour.size());
    if (!numbers) return Support::Unexpected(numbers.error());
    std::array<uint8_t, 4> colour{};
    for (std::size_t i = 0; i < colour.size(); i++) {
        const int64_t channel = (*numbers)[i];
        if (channel < 0 || std::cmp_greater(channel, std::numeric_limits<uint8_t>::max()))
            return Support::Unexpected(std::to_string(channel) + " is not a colour channel");
        colour.at(i) = static_cast<uint8_t>(channel);
    }
    animation.background_colour = colour;
    return {};
}

Support::Expected<void, std::string> SetUseBackground(AfpAnimation::Animation& animation,
                                                      std::string_view value) {
    if (value == kOn) {
        animation.flags |= kColourFlag;
        return {};
    }
    if (value == kOff) {
        animation.flags &= ~kColourFlag;
        return {};
    }
    return Support::Unexpected(std::string(kUseBackground) + " is " + std::string(kOn) + " or " +
                               std::string(kOff));
}

}

StageSize StageSizeOf(const AfpAnimation::Animation& animation) {
    return StageSize{.width = animation.rect[kMaxX] - animation.rect[kMinX],
                     .height = animation.rect[kMaxY] - animation.rect[kMinY]};
}

std::vector<Field> AnimationSettingFields(const AfpAnimation::Animation& animation) {
    std::vector<std::string> colour;
    colour.reserve(animation.background_colour.size());
    for (const uint8_t channel : animation.background_colour)
        colour.push_back(std::to_string(channel));
    const StageSize stage = StageSizeOf(animation);
    return {
        Field{.name = std::string(kStageSize),
              .value = Join({std::to_string(stage.width), std::to_string(stage.height)})},
        Field{.name = std::string(kFrameRate), .value = RateText(animation)},
        Field{.name = std::string(kBackgroundColour), .value = Join(colour)},
        Field{.name = std::string(kUseBackground),
              .value = std::string((animation.flags & kColourFlag) != 0 ? kOn : kOff)},
    };
}

Support::Expected<void, std::string> SetAnimationSetting(AfpAnimation::Animation& animation,
                                                         std::string_view name,
                                                         std::string_view value) {
    AfpAnimation::Animation edited = animation;
    Support::Expected<void, std::string> set;
    if (name == kStageSize) {
        set = SetStageSize(edited, value);
    } else if (name == kFrameRate) {
        set = SetFrameRate(edited, value);
    } else if (name == kBackgroundColour) {
        set = SetBackgroundColour(edited, value);
    } else if (name == kUseBackground) {
        set = SetUseBackground(edited, value);
    } else {
        return Support::Unexpected(std::string(name) + " is not an animation setting");
    }
    if (!set) return Support::Unexpected(set.error());
    animation = edited;
    return {};
}

}
