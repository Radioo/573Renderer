#pragma once

#include "document/clip.h"
#include "formats/afp_animation.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Document {

enum class InputDriven : uint8_t { Texture, Frames };

struct InputSlot {
    std::string name;
    ClipId clip;
    uint16_t depth = 0;
    uint32_t frame = 0;
    std::optional<uint16_t> character;
    std::string texture;
    uint32_t frames = 0;
    std::size_t places = 0;
    InputDriven driven = InputDriven::Texture;

    friend bool operator==(const InputSlot&, const InputSlot&) = default;
};

struct InputLabel {
    std::string name;
    ClipId clip;
    uint32_t frame = 0;

    friend bool operator==(const InputLabel&, const InputLabel&) = default;
};

struct InputSurface {
    std::vector<InputSlot> names;
    std::vector<InputLabel> labels;

    friend bool operator==(const InputSurface&, const InputSurface&) = default;
};

struct InputNumber {
    std::string stem;
    std::size_t digit_at = 0;
    std::vector<std::size_t> places;
    std::vector<uint32_t> weights;
};

[[nodiscard]] InputSurface Inputs(const AfpAnimation::Animation& animation,
                                  const std::map<uint16_t, std::string>& shape_images);

[[nodiscard]] std::vector<InputNumber> Numbers(const InputSurface& surface,
                                               const std::vector<std::string>& images);

[[nodiscard]] std::string Digit(const std::string& texture, std::size_t digit_at, uint32_t digit);

[[nodiscard]] uint32_t DigitOf(const std::string& texture, std::size_t digit_at);

}
