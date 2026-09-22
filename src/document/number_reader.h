#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace Document {

class NumberReader {
public:
    explicit NumberReader(std::span<const int64_t> numbers) : numbers_(numbers) {}

    [[nodiscard]] bool Done() const { return at_ >= numbers_.size(); }

    [[nodiscard]] std::size_t Left() const { return numbers_.size() - at_; }

    template <typename T> std::optional<T> Next() {
        if (at_ >= numbers_.size()) return std::nullopt;
        const int64_t value = numbers_[at_++];
        if (value < static_cast<int64_t>(std::numeric_limits<T>::min()) ||
            value > static_cast<int64_t>(std::numeric_limits<T>::max()))
            return std::nullopt;
        return static_cast<T>(value);
    }

    std::optional<std::vector<uint8_t>> Bytes() {
        const std::optional<uint32_t> size = Next<uint32_t>();
        if (!size || *size > Left()) return std::nullopt;
        std::vector<uint8_t> out;
        out.reserve(*size);
        for (uint32_t i = 0; i < *size; i++) {
            const std::optional<uint8_t> byte = Next<uint8_t>();
            if (!byte) return std::nullopt;
            out.push_back(*byte);
        }
        return out;
    }

    template <typename T, std::size_t N> bool Fill(std::array<T, N>& out) {
        for (T& value : out) {
            const std::optional<T> next = Next<T>();
            if (!next) return false;
            value = *next;
        }
        return true;
    }

private:
    std::span<const int64_t> numbers_;
    std::size_t at_ = 0;
};

}
