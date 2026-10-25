#pragma once

#include "support/expected.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace AfpByteOrder {

struct Swap {
    std::size_t offset = 0;
    uint8_t element_size = 0;
    std::size_t count = 0;

    friend bool operator==(const Swap&, const Swap&) = default;
};

struct Restored {
    std::vector<uint8_t> data;
    std::vector<Swap> swaps;
    bool strings_scrambled = false;
};

[[nodiscard]] Support::Expected<std::vector<Swap>, std::string>
ReadScript(std::span<const uint8_t> script);

[[nodiscard]] Support::Expected<std::vector<uint8_t>, std::string>
WriteScript(std::span<const Swap> swaps);

[[nodiscard]] Support::Expected<Restored, std::string> Restore(std::span<const uint8_t> stored,
                                                               std::span<const uint8_t> script);

[[nodiscard]] Support::Expected<std::vector<uint8_t>, std::string>
Store(std::span<const uint8_t> native, std::span<const Swap> swaps, bool scramble_strings);

}
