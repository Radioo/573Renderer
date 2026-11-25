#include "judge/judge_backdrop.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string_view>

namespace Judge {

namespace {

constexpr int kSquarePixels = 16;
constexpr std::uint32_t kBlack = 0xFF000000U;
constexpr std::uint32_t kGrey = 0xFF808080U;
constexpr std::uint32_t kWhite = 0xFFFFFFFFU;
constexpr std::uint32_t kRed = 0xFFFF0000U;
constexpr std::uint32_t kGreen = 0xFF00FF00U;
constexpr std::uint32_t kBlue = 0xFF0000FFU;
constexpr std::uint32_t kCheckerLight = 0xFFFFFFFFU;
constexpr std::uint32_t kCheckerDark = 0xFFCCCCCCU;

std::uint32_t Solid(Backdrop backdrop) {
    switch (backdrop) {
    case Backdrop::Black:
        return kBlack;
    case Backdrop::Grey:
        return kGrey;
    case Backdrop::White:
        return kWhite;
    case Backdrop::Red:
        return kRed;
    case Backdrop::Green:
        return kGreen;
    case Backdrop::Blue:
        return kBlue;
    case Backdrop::Checkerboard:
        return kCheckerLight;
    }
    return kBlack;
}

}

BackdropFill FillFor(Backdrop backdrop, int width, int height) {
    BackdropFill fill;
    fill.base = Solid(backdrop);
    if (backdrop != Backdrop::Checkerboard) return fill;
    fill.squares = kCheckerDark;
    for (int y = 0; y < height; y += kSquarePixels) {
        for (int x = 0; x < width; x += kSquarePixels) {
            if (((x / kSquarePixels) + (y / kSquarePixels)) % 2 == 0) continue;
            fill.rects.push_back({.x0 = x,
                                  .y0 = y,
                                  .x1 = std::min(x + kSquarePixels, width),
                                  .y1 = std::min(y + kSquarePixels, height)});
        }
    }
    return fill;
}

std::optional<Backdrop> BackdropByName(std::string_view name) {
    for (const Backdrop backdrop : kBackdrops) {
        const std::string_view known = BackdropName(backdrop);
        const bool same = std::ranges::equal(name, known, [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) ==
                   std::tolower(static_cast<unsigned char>(b));
        });
        if (same) return backdrop;
    }
    return std::nullopt;
}

const char* BackdropName(Backdrop backdrop) {
    switch (backdrop) {
    case Backdrop::Black:
        return "Black";
    case Backdrop::Grey:
        return "Grey";
    case Backdrop::White:
        return "White";
    case Backdrop::Red:
        return "Red";
    case Backdrop::Green:
        return "Green";
    case Backdrop::Blue:
        return "Blue";
    case Backdrop::Checkerboard:
        return "Checker";
    }
    return "Black";
}

}
