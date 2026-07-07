#pragma once

#include <tl/expected.hpp>

#include <utility>

namespace Support {

template <typename T, typename E> using Expected = tl::expected<T, E>;

template <typename E> [[nodiscard]] auto Unexpected(E&& e) {
    return tl::make_unexpected(std::forward<E>(e));
}

}
