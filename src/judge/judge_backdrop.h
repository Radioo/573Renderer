#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace Judge {

enum class Backdrop : std::uint8_t { Black, Grey, White, Red, Green, Blue, Checkerboard };

inline constexpr std::array<Backdrop, 7> kBackdrops{
    Backdrop::Black, Backdrop::Grey, Backdrop::White,       Backdrop::Red,
    Backdrop::Green, Backdrop::Blue, Backdrop::Checkerboard};

struct BackdropRect {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
};

struct BackdropFill {
    std::uint32_t base = 0;
    std::uint32_t squares = 0;
    std::vector<BackdropRect> rects;
};

[[nodiscard]] BackdropFill FillFor(Backdrop backdrop, int width, int height);
[[nodiscard]] const char* BackdropName(Backdrop backdrop);
[[nodiscard]] std::optional<Backdrop> BackdropByName(std::string_view name);

}
