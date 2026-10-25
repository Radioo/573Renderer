#pragma once

#include "support/expected.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Document {

[[nodiscard]] std::string Join(const std::vector<std::string>& parts);

[[nodiscard]] Support::Expected<std::vector<int64_t>, std::string> Numbers(std::string_view value,
                                                                           std::size_t count);

template <typename T> std::string Scalar(const std::optional<T>& field) {
    return field ? std::to_string(*field) : std::string();
}

template <typename T, std::size_t N>
std::string Vector(const std::optional<std::array<T, N>>& field) {
    if (!field) return {};
    std::vector<std::string> parts;
    parts.reserve(N);
    for (const T value : *field)
        parts.push_back(std::to_string(value));
    return Join(parts);
}

template <typename T>
Support::Expected<void, std::string> SetScalar(std::optional<T>& field, std::string_view value) {
    if (value.empty()) {
        field.reset();
        return {};
    }
    auto numbers = Numbers(value, 1);
    if (!numbers) return Support::Unexpected(numbers.error());
    const int64_t number = numbers->front();
    if (number < static_cast<int64_t>(std::numeric_limits<T>::min()) ||
        number > static_cast<int64_t>(std::numeric_limits<T>::max())) {
        return Support::Unexpected(std::to_string(number) + " does not fit the field");
    }
    field = static_cast<T>(number);
    return {};
}

template <typename T, std::size_t N>
Support::Expected<void, std::string> SetVector(std::optional<std::array<T, N>>& field,
                                               std::string_view value) {
    if (value.empty()) {
        field.reset();
        return {};
    }
    auto numbers = Numbers(value, N);
    if (!numbers) return Support::Unexpected(numbers.error());
    std::array<T, N> out{};
    for (std::size_t i = 0; i < N; i++) {
        const int64_t number = (*numbers)[i];
        if (number < static_cast<int64_t>(std::numeric_limits<T>::min()) ||
            number > static_cast<int64_t>(std::numeric_limits<T>::max())) {
            return Support::Unexpected(std::to_string(number) + " does not fit the field");
        }
        out[i] = static_cast<T>(number);
    }
    field = out;
    return {};
}

}
