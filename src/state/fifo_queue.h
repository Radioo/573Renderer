#pragma once

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

namespace App {

template <typename T> class FifoQueue {
public:
    void Post(T item) {
        const std::scoped_lock lk(mu_);
        items_.push_back(std::move(item));
    }

    [[nodiscard]] std::optional<T> Take() {
        const std::scoped_lock lk(mu_);
        if (items_.empty()) return std::nullopt;
        T out = std::move(items_.front());
        items_.pop_front();
        return out;
    }

    [[nodiscard]] std::size_t Size() const {
        const std::scoped_lock lk(mu_);
        return items_.size();
    }

private:
    mutable std::mutex mu_;
    std::deque<T> items_;
};

}
